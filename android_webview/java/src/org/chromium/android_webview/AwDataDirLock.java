// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.Process;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Handles locking the WebView's data directory, to prevent concurrent use from more than one
 * process.
 */
public abstract class AwDataDirLock {
    private static final String TAG = "AwDataDirLock";

    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";

    // This results in a maximum wait time of 1.5s
    private static final int LOCK_RETRIES = 16;
    private static final int LOCK_SLEEP_MS = 100;

    private static @Nullable RandomAccessFile sLockFile;
    private static @Nullable FileLock sExclusiveFileLock;

    @IntDef({
        ProcessStatus.PROBE_FAILURE,
        ProcessStatus.STATUS_FAILURE,
        ProcessStatus.NONEXISTENT,
        ProcessStatus.INACCESSIBLE,
        ProcessStatus.UNINTERRUPTIBLE,
        ProcessStatus.ZOMBIE,
        ProcessStatus.ALIVE,
    })
    private @interface ProcessStatus {
        // Calling Os.kill(pid, 0) failed in an unexpected way.
        int PROBE_FAILURE = 0;
        // Reading and parsing /proc/<pid>/stat failed.
        int STATUS_FAILURE = 1;
        // The process doesn't appear to exist (but may just be hidden from us).
        int NONEXISTENT = 2;
        // The process appears to exist but is inaccessible to us.
        int INACCESSIBLE = 3;
        // The process's main thread is in uninterruptible sleep (D state).
        int UNINTERRUPTIBLE = 4;
        // The process's main thread has exited (Z state).
        int ZOMBIE = 5;
        // The process is alive (any other state).
        int ALIVE = 6;
    }

    public static void lock(final Context appContext) {
        try (DualTraceEvent e1 = DualTraceEvent.scoped("AwDataDirLock.lock");
                StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            if (sExclusiveFileLock != null) {
                // We have already called lock() and successfully acquired the lock in this process.
                // This can happen if our own code calls lock() multiple times or it could be the
                // result of an app catching an exception thrown during initialization and
                // discarding it, causing us to later attempt to initialize WebView again.
                return;
            }

            // If we already called lock() but didn't succeed in getting the lock, it's possible the
            // app caught the exception and tried again later. As above, there's no real advantage
            // to failing here, so only open the lock file if we didn't already open it before.
            if (sLockFile == null) {
                String dataPath = PathUtils.getDataDirectory();
                File lockFile = new File(dataPath, EXCLUSIVE_LOCK_FILE);

                try {
                    // Note that the file is kept open intentionally.
                    sLockFile = new RandomAccessFile(lockFile, "rw");
                } catch (IOException e) {
                    // Failing to create the lock file is always fatal; even if multiple processes
                    // are using the same data directory we should always be able to access the file
                    // itself.
                    throw new RuntimeException("Failed to create lock file " + lockFile, e);
                }
            }

            // Android doesn't guarantee that there aren't two copies of the same app process alive
            // at the same time - it doesn't ever do this intentionally, but the request to kill the
            // old process doesn't happen instantaneously, so it can *think* it's killed the old
            // process while actually it still exists as far as the kernel is concerned, and since
            // file locks are process-level the old process can still be holding the lock. The app
            // isn't doing anything wrong in this case - they only have to ensure that *different*
            // processes they create use distinct data directories.
            //
            // On older Android versions this edge case just isn't handled at all. On Android 11+
            // the system does have logic to wait for the confirmation that the old process has
            // actually exited before launching another one, but the wait has a timeout, so it can
            // still happen.
            //
            // We retry the lock a few times here in the hope that the old process will actually
            // exit and the lock will be released, but it seems like in the vast majority of cases
            // the old process will *never* exit and will hang around until the device is actually
            // fully rebooted. This can happen if one of the process's threads is in uninterruptible
            // sleep in the kernel ("D state"): the kill is deferred until the thread wakes up from
            // sleep, but there's a long history of bugs in the kernel code (generally a device
            // driver or filesystem rather than the "core" kernel logic) that can result in wakeups
            // being lost and the thread being stuck forever.
            //
            // We could just ignore the lock in this case; if the other process is actually stuck in
            // D state forever then it's not going to run our code any more and can't actually cause
            // any real data corruption issues. But, this would be somewhat risky: various parts of
            // Chromium *also* use file locks on individual files within the data directory (e.g.
            // sqlite databases) so just ignoring the lock failure here may lead to a similar lock
            // failure later for another file, which may block forever, or crash in native code in a
            // way that's harder to identify and debug than the specific Java exception we throw
            // here. So, for now we continue to throw an exception if we run out of retries.
            //
            // This is really annoying for apps since it's not their fault and they can't do
            // anything to prevent or avoid it other than catch exceptions from WebView startup and
            // give up on using it, but we don't support or recommend this, and it's difficult to
            // implement correctly without introducing other problems.
            for (int attempts = 1; attempts <= LOCK_RETRIES; ++attempts) {
                try {
                    sExclusiveFileLock = sLockFile.getChannel().tryLock();
                } catch (IOException e) {
                    // Older versions of Android incorrectly throw IOException when the flock()
                    // call fails with EAGAIN, instead of returning null. Just ignore the exception
                    // and continue as if it did return null.
                } catch (Exception e) {
                    // This is primarily intended to catch OverlappingFileLockException as described
                    // below, but it seems reasonable to apply the same treatment to any exception.
                    //
                    // The Java standard library maintains its own internal file lock table in
                    // addition to using the actual flock() system call, because it wants to have
                    // portable/consistent behavior in the case where two different threads in the
                    // same process both try to lock the same file concurrently.
                    //
                    // This shouldn't ever happen because this code is always called on the UI
                    // thread, but unfortunately there is at least one library that opens our
                    // lockfile and tries to lock it itself as part of an ill-advised workaround for
                    // data directory locking issues, and this can race with init on another thread.
                    //
                    // Rather than just letting this crash, we treat it the same way as a normal
                    // failure to acquire the lock - we expect the other code to release the lock
                    // quickly, so retrying will probably resolve the situation. If we're on our
                    // last retry, though, we rethrow the exception so that we still see the real
                    // cause in crash data instead of just a generic failure to acquire the lock.
                    if (attempts == LOCK_RETRIES) {
                        throw e;
                    }
                }
                if (sExclusiveFileLock != null) {
                    // We got the lock; write out info for debugging.
                    ProcessInfo.current().writeToFile(sLockFile);
                    return;
                }

                // If we're not out of retries, sleep and try again.
                if (attempts == LOCK_RETRIES) break;
                try {
                    Thread.sleep(LOCK_SLEEP_MS);
                } catch (InterruptedException e) {
                }
            }

            // We failed to get the lock even after retrying.
            @Nullable ProcessInfo holder = ProcessInfo.readFromFile(sLockFile);
            String error = getLockFailureReason(holder);
            if (CompatQuirks.isEnabled(CompatQuirks.Quirk.DATA_DIRECTORY_LOCK_WARN_ONLY)) {
                Log.w(TAG, error);
            } else {
                throw new RuntimeException(error);
            }
        }
    }

    private static class ProcessInfo {
        public final int pid;
        public final String processName;
        public final @ProcessStatus int processStatus;

        private ProcessInfo(int pid, String processName, @ProcessStatus int processStatus) {
            this.pid = pid;
            this.processName = processName;
            this.processStatus = processStatus;
        }

        @Override
        public String toString() {
            return processName + " (pid " + pid + ")";
        }

        static ProcessInfo current() {
            return new ProcessInfo(
                    Process.myPid(), ContextUtils.getProcessName(), ProcessStatus.ALIVE);
        }

        static @Nullable ProcessInfo readFromFile(RandomAccessFile file) {
            try {
                int pid = file.readInt();
                String processName = file.readUTF();
                return new ProcessInfo(pid, processName, getProcessStatus(pid));
            } catch (Exception e) {
                // Failed to read the debugging info.
                return null;
            }
        }

        void writeToFile(RandomAccessFile file) {
            try {
                // Truncate the file first to get rid of old data.
                file.setLength(0);
                file.writeInt(pid);
                file.writeUTF(processName);
            } catch (Exception e) {
                // Don't crash just because something failed here, as it's only for debugging.
                Log.w(TAG, "Failed to write info to lock file", e);
            }
        }

        static String formatWithStatus(@Nullable ProcessInfo info) {
            if (info == null) {
                return "unknown";
            }

            return info.toString()
                    + switch (info.processStatus) {
                        case ProcessStatus.PROBE_FAILURE -> " probe failed!";
                        case ProcessStatus.STATUS_FAILURE -> " status read failed!";
                        case ProcessStatus.NONEXISTENT -> " doesn't exist!";
                        case ProcessStatus.INACCESSIBLE -> " pid has been reused!";
                        case ProcessStatus.UNINTERRUPTIBLE -> " is uninterruptible";
                        case ProcessStatus.ZOMBIE -> " is a zombie";
                        case ProcessStatus.ALIVE -> " is alive";
                        default -> " status unknown!";
                    };
        }

        private static @ProcessStatus int getProcessStatus(int pid) {
            try {
                // Check the status of the pid holding the lock by sending it a null signal.
                // This doesn't actually send a signal, just runs the kernel access checks.
                Os.kill(pid, 0);
            } catch (ErrnoException e) {
                if (e.errno == OsConstants.ESRCH) {
                    return ProcessStatus.NONEXISTENT;
                } else if (e.errno == OsConstants.EPERM) {
                    return ProcessStatus.INACCESSIBLE;
                } else {
                    return ProcessStatus.PROBE_FAILURE;
                }
            } catch (Exception e) {
                return ProcessStatus.PROBE_FAILURE;
            }

            // No exception means the process exists and has the same uid as us, so is
            // probably an instance of the same app. We should be able to access its
            // status in /proc to determine a more specific state.
            try {
                Path statPath = new File("/proc/" + pid + "/stat").toPath();
                byte[] stat = Files.readAllBytes(statPath);
                // Stick with bytes - there are no guarantees about string encoding here.
                // The last ASCII ')' in stat is the end of the process name field, followed
                // by a space and then an ASCII letter for the process status.
                for (int i = stat.length - 1; i >= 0; i--) {
                    if (stat[i] == (byte) ')') {
                        if (i + 2 < stat.length) {
                            return switch ((char) stat[i + 2]) {
                                case 'D' -> ProcessStatus.UNINTERRUPTIBLE;
                                case 'Z' -> ProcessStatus.ZOMBIE;
                                default -> ProcessStatus.ALIVE;
                            };
                        }
                        break;
                    }
                }
            } catch (Exception e) {
            }
            return ProcessStatus.STATUS_FAILURE;
        }
    }

    private static String getLockFailureReason(@Nullable ProcessInfo holder) {
        final StringBuilder error =
                new StringBuilder(
                        "Using WebView from more than one process at once with the same data"
                                + " directory is not supported. https://crbug.com/558377 : Current"
                                + " process ");
        error.append(ProcessInfo.current().toString());
        error.append(", lock owner ");
        error.append(ProcessInfo.formatWithStatus(holder));
        return error.toString();
    }
}
