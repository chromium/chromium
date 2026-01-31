// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.Build;
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
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;

/**
 * Handles locking the WebView's data directory, to prevent concurrent use from more than one
 * process.
 */
abstract class AwDataDirLock {
    private static final String TAG = "AwDataDirLock";

    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";

    // This results in a maximum wait time of 1.5s
    private static final int LOCK_RETRIES = 16;
    private static final int LOCK_SLEEP_MS = 100;

    private static @Nullable RandomAccessFile sLockFile;
    private static @Nullable FileLock sExclusiveFileLock;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // This is an enum histogram and not a number, because we are more
    // interested in whether retrying was necessary *at all* and whether the
    // overlapping file lock issue was involved than the exact retry count;
    // a general range is enough to tweak LOCK_RETRIES.
    @IntDef({
        LockRetryResult.FIRST_TIME_OK,
        LockRetryResult.ONE_RETRY_NO_OVERLAP,
        LockRetryResult.ONE_RETRY_WITH_OVERLAP,
        LockRetryResult.UP_TO_THREE_RETRIES_NO_OVERLAP,
        LockRetryResult.UP_TO_THREE_RETRIES_WITH_OVERLAP,
        LockRetryResult.MORE_RETRIES_NO_OVERLAP,
        LockRetryResult.MORE_RETRIES_WITH_OVERLAP,
    })
    private @interface LockRetryResult {
        int FIRST_TIME_OK = 0;
        int ONE_RETRY_NO_OVERLAP = 1;
        int ONE_RETRY_WITH_OVERLAP = 2;
        int UP_TO_THREE_RETRIES_NO_OVERLAP = 3;
        int UP_TO_THREE_RETRIES_WITH_OVERLAP = 4;
        int MORE_RETRIES_NO_OVERLAP = 5;
        int MORE_RETRIES_WITH_OVERLAP = 6;
        int COUNT = 7;
    }

    private static void logResult(@LockRetryResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.LockRetryResult", result, LockRetryResult.COUNT);
    }

    static void lock(final Context appContext) {
        try (DualTraceEvent e1 = DualTraceEvent.scoped("AwDataDirLock.lock");
                StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            if (sExclusiveFileLock != null) {
                // We have already called lock() and successfully acquired the lock in this process.
                // This shouldn't happen, but is likely to be the result of an app catching an
                // exception thrown during initialization and discarding it, causing us to later
                // attempt to initialize WebView again. There's no real advantage to failing the
                // locking code when this happens; we may as well count this as the lock being
                // acquired and let init continue (though the app may experience other problems
                // later).
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
            boolean sawOverlappingFileLock = false;
            for (int attempts = 1; attempts <= LOCK_RETRIES; ++attempts) {
                try {
                    sExclusiveFileLock = sLockFile.getChannel().tryLock();
                } catch (IOException e) {
                    // Older versions of Android incorrectly throw IOException when the flock()
                    // call fails with EAGAIN, instead of returning null. Just ignore the exception
                    // and continue as if it did return null.
                } catch (OverlappingFileLockException e) {
                    // The Java standard library maintains its own internal file lock table in
                    // addition to using the actual flock() system call, because it wants to have
                    // portable/consistent behavior in the case where two different threads in the
                    // same process both try to lock the same file concurrently - whether this would
                    // succeed or fail is OS-dependent. This is the exception thrown for that case.
                    //
                    // In theory this can't ever happen because we only touch the lock file from
                    // this method, which is always called on the UI thread and so can't race with
                    // itself.
                    //
                    // In reality, there is at least one known case of library code that opens
                    // *our lockfile* and tries to lock it itself, to determine in advance whether
                    // initializing WebView is going to fail due to a failure to acquire the lock!
                    // This is ill-advised for many reasons, but the most important is that the
                    // culprit code can run on any thread and thus can race with our locking code
                    // if another thread in the app has concurrently triggered WebView
                    // initialization without messing with our lockfile first. When this happens,
                    // the culprit code that's trying to *prevent* a lock failure crash can end up
                    // *causing* a lock failure crash that otherwise would not have happened.
                    //
                    // This also isn't really the app developer's fault because the known cases are
                    // in closed-source 3P libraries. So, until we reach the last retry, we treat
                    // this the same as a normal lock failure. If the cause really was other code
                    // messing with our lockfile then we expect it's going to release the lock again
                    // very quickly so that it can try to use WebView and we'll succeed next time.
                    // On the last retry, we rethrow the exception, because we don't want to lose
                    // the crash report data that shows us that the OverlappingFileLockException is
                    // occuring by having it just turn into the normal lock failure exception.
                    sawOverlappingFileLock = true;
                    if (attempts == LOCK_RETRIES) {
                        throw e;
                    }
                }
                if (sExclusiveFileLock != null) {
                    // We got the lock; write out info for debugging.
                    ProcessInfo.current().writeToFile(sLockFile);

                    // Log the appropriate metric value to track whether the retry mechanism is
                    // actually doing anything useful for apps. We only log it here - there's no
                    // point in logging it when the locking fails because we're going to throw an
                    // exception and fail startup anyway, so the metric would never actually be
                    // uploaded; persistent histograms are not yet initialized at this time.
                    if (attempts == 1) {
                        logResult(LockRetryResult.FIRST_TIME_OK);
                    } else if (attempts == 2) {
                        if (sawOverlappingFileLock) {
                            logResult(LockRetryResult.ONE_RETRY_WITH_OVERLAP);
                        } else {
                            logResult(LockRetryResult.ONE_RETRY_NO_OVERLAP);
                        }
                    } else if (attempts <= 4) {
                        if (sawOverlappingFileLock) {
                            logResult(LockRetryResult.UP_TO_THREE_RETRIES_WITH_OVERLAP);
                        } else {
                            logResult(LockRetryResult.UP_TO_THREE_RETRIES_NO_OVERLAP);
                        }
                    } else {
                        if (sawOverlappingFileLock) {
                            logResult(LockRetryResult.MORE_RETRIES_WITH_OVERLAP);
                        } else {
                            logResult(LockRetryResult.MORE_RETRIES_NO_OVERLAP);
                        }
                    }

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
            // Many existing apps rely on this even though it's known to be unsafe.
            // Make it fatal for apps that target P or higher
            @Nullable ProcessInfo holder = ProcessInfo.readFromFile(sLockFile);
            String error = getLockFailureReason(holder);
            if (appContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                throw new RuntimeException(error);
            } else {
                Log.w(TAG, error);
            }
        }
    }

    private static class ProcessInfo {
        public final int pid;
        public final String processName;

        private ProcessInfo(int pid, String processName) {
            this.pid = pid;
            this.processName = processName;
        }

        @Override
        public String toString() {
            return processName + " (pid " + pid + ")";
        }

        static ProcessInfo current() {
            return new ProcessInfo(Process.myPid(), ContextUtils.getProcessName());
        }

        static @Nullable ProcessInfo readFromFile(RandomAccessFile file) {
            try {
                int pid = file.readInt();
                String processName = file.readUTF();
                return new ProcessInfo(pid, processName);
            } catch (IOException e) {
                // We'll get IOException if we failed to read the pid and process name; e.g. if the
                // lockfile is from an old version of WebView or an IO error occurred somewhere.
                return null;
            }
        }

        void writeToFile(RandomAccessFile file) {
            try {
                // Truncate the file first to get rid of old data.
                file.setLength(0);
                file.writeInt(pid);
                file.writeUTF(processName);
            } catch (IOException e) {
                // Don't crash just because something failed here, as it's only for debugging.
                Log.w(TAG, "Failed to write info to lock file", e);
            }
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
        if (holder != null) {
            error.append(holder.toString());

            // Check the status of the pid holding the lock by sending it a null signal.
            // This doesn't actually send a signal, just runs the kernel access checks.
            try {
                Os.kill(holder.pid, 0);

                // No exception means the process exists and has the same uid as us, so is
                // probably an instance of the same app. Leave the message alone.
            } catch (ErrnoException e) {
                if (e.errno == OsConstants.ESRCH) {
                    // pid did not exist - the lock should have been released by the kernel,
                    // so this process info is probably wrong.
                    error.append(" doesn't exist!");
                } else if (e.errno == OsConstants.EPERM) {
                    // pid existed but didn't have the same uid as us.
                    // Most likely the pid has just been recycled for a new process
                    error.append(" pid has been reused!");
                } else {
                    // EINVAL is the only other documented return value for kill(2) and should never
                    // happen for signal 0, so just complain generally.
                    error.append(" status unknown!");
                }
            }
        } else {
            error.append(" unknown");
        }
        return error.toString();
    }
}
