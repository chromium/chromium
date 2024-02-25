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

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.ScopedSysTraceEvent;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

/**
 * Handles locking the WebView's data directory, to prevent concurrent use from
 * more than one process.
 */
abstract class AwDataDirLock {
    private static final String TAG = "AwDataDirLock";

    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";

    // This results in a maximum wait time of 1.5s
    private static final int LOCK_RETRIES = 16;
    private static final int LOCK_SLEEP_MS = 100;

    private static @Nullable RandomAccessFile sLockFile;
    private static @Nullable FileLock sExclusiveFileLock;

    static void lock(final Context appContext) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("AwDataDirLock.lock");
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

            // Android versions before 11 have edge cases where a new instance of an app process can
            // be started while an existing one is still in the process of being killed. This can
            // still happen on Android 11+ because the platform has a timeout for waiting, but it's
            // much less likely. Retry the lock a few times to give the old process time to fully go
            // away.
            for (int attempts = 1; attempts <= LOCK_RETRIES; ++attempts) {
                try {
                    sExclusiveFileLock = sLockFile.getChannel().tryLock();
                } catch (IOException e) {
                    // Older versions of Android incorrectly throw IOException when the flock()
                    // call fails with EAGAIN, instead of returning null. Just ignore it.
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
            // Many existing apps rely on this even though it's known to be unsafe.
            // Make it fatal when on P for apps that target P or higher
            @Nullable ProcessInfo holder = ProcessInfo.readFromFile(sLockFile);
            String error = getLockFailureReason(holder);
            boolean dieOnFailure =
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                            && appContext.getApplicationInfo().targetSdkVersion
                                    >= Build.VERSION_CODES.P;
            if (dieOnFailure) {
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
