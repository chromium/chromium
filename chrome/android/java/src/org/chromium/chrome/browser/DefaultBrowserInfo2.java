// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A utility class for querying information about the default browser setting.
 * TODO(crbug.com/40709747): Remove DefaultBrowserInfo and replace with this.
 */
public final class DefaultBrowserInfo2 {
    /** Contains all status related to the default browser state on the device. */
    public static class DefaultInfo {
        /** Whether or not Chrome is the system browser. */
        public final boolean isChromeSystem;

        /** Whether or not Chrome is the default browser. */
        public final boolean isChromeDefault;

        /** Whether or not the default browser is the system browser. */
        public final boolean isDefaultSystem;

        /** Whether or not the user has set a default browser. */
        public final boolean hasDefault;

        /** The number of browsers installed on this device. */
        public final int browserCount;

        /** The number of system browsers installed on this device. */
        public final int systemCount;

        /** Creates an instance of the {@link DefaultInfo} class. */
        public DefaultInfo(
                boolean isChromeSystem,
                boolean isChromeDefault,
                boolean isDefaultSystem,
                boolean hasDefault,
                int browserCount,
                int systemCount) {
            this.isChromeSystem = isChromeSystem;
            this.isChromeDefault = isChromeDefault;
            this.isDefaultSystem = isDefaultSystem;
            this.hasDefault = hasDefault;
            this.browserCount = browserCount;
            this.systemCount = systemCount;
        }
    }

    private static DefaultInfoTask sDefaultInfoTask;

    /** Don't instantiate me. */
    private DefaultBrowserInfo2() {}

    /**
     * Determines various information about browsers on the system.
     * @param callback To be called with a {@link DefaultInfo} instance if possible.  Can be {@code
     *         null}.
     * @see DefaultInfo
     */
    public static void getDefaultBrowserInfo(Callback<DefaultInfo> callback) {
        ThreadUtils.checkUiThread();
        if (sDefaultInfoTask == null) sDefaultInfoTask = new DefaultInfoTask();
        sDefaultInfoTask.get(callback);
    }

    public static void setDefaultInfoForTests(DefaultInfo info) {
        DefaultInfoTask.setDefaultInfoForTests(info);
    }

    public static void clearDefaultInfoForTests() {
        DefaultInfoTask.clearDefaultInfoForTests();
    }

    private static class DefaultInfoTask extends AsyncTask<DefaultInfo> {
        private static AtomicReference<DefaultInfo> sTestInfo;

        private final ObserverList<Callback<DefaultInfo>> mObservers = new ObserverList<>();

        public static void setDefaultInfoForTests(DefaultInfo info) {
            sTestInfo = new AtomicReference<DefaultInfo>(info);
        }

        public static void clearDefaultInfoForTests() {
            sTestInfo = null;
        }

        /**
         *  Queues up {@code callback} to be notified of the result of this {@link AsyncTask}.  If
         *  the task has not been started, this will start it.  If the task is finished, this will
         *  send the result.  If the task is running this will queue the callback up until the task
         *  is done.
         *
         * @param callback The {@link Callback} to notify with the right {@link DefaultInfo}.
         */
        public void get(Callback<DefaultInfo> callback) {
            ThreadUtils.checkUiThread();

            if (getStatus() == Status.FINISHED) {
                DefaultInfo info = null;
                try {
                    info = sTestInfo == null ? get() : sTestInfo.get();
                } catch (InterruptedException | ExecutionException e) {
                    // Fail silently here since this is not a critical task.
                }

                final DefaultInfo postInfo = info;
                PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(postInfo));
            } else {
                if (getStatus() == Status.PENDING) {
                    try {
                        executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    } catch (RejectedExecutionException e) {
                        // Fail silently here since this is not a critical task.
                        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
                        return;
                    }
                }
                mObservers.addObserver(callback);
            }
        }

        @Override
        protected DefaultInfo doInBackground() {
            Context context = ContextUtils.getApplicationContext();

            boolean isChromeSystem = false;
            boolean isChromeDefault = false;
            boolean isDefaultSystem = false;
            boolean hasDefault = false;
            int browserCount = 0;
            int systemCount = 0;

            // Query the default handler first.
            ResolveInfo defaultRi = PackageManagerUtils.resolveDefaultWebBrowserActivity();
            if (defaultRi != null && defaultRi.match != 0) {
                hasDefault = true;
                isChromeDefault = isSamePackage(context, defaultRi);
                isDefaultSystem = isSystemPackage(defaultRi);
            }

            // Query all other intent handlers.
            Set<String> uniquePackages = new HashSet<>();
            List<ResolveInfo> ris = PackageManagerUtils.queryAllWebBrowsersInfo();
            if (ris != null) {
                for (ResolveInfo ri : ris) {
                    String packageName = ri.activityInfo.applicationInfo.packageName;
                    if (!uniquePackages.add(packageName)) continue;

                    if (isSystemPackage(ri)) {
                        if (isSamePackage(context, ri)) isChromeSystem = true;
                        systemCount++;
                    }
                }
            }

            browserCount = uniquePackages.size();

            return new DefaultInfo(
                    isChromeSystem,
                    isChromeDefault,
                    isDefaultSystem,
                    hasDefault,
                    browserCount,
                    systemCount);
        }

        @Override
        protected void onPostExecute(DefaultInfo defaultInfo) {
            flushCallbacks(sTestInfo == null ? defaultInfo : sTestInfo.get());
        }

        @Override
        protected void onCancelled() {
            flushCallbacks(null);
        }

        private void flushCallbacks(DefaultInfo info) {
            for (Callback<DefaultInfo> callback : mObservers) callback.onResult(info);
            mObservers.clear();
        }
    }

    private static boolean isSamePackage(Context context, ResolveInfo info) {
        return TextUtils.equals(
                context.getPackageName(), info.activityInfo.applicationInfo.packageName);
    }

    private static boolean isSystemPackage(ResolveInfo info) {
        return (info.activityInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }
}
