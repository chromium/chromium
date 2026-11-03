// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import android.content.Context;

import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupMetrics;

/**
 * Entry point for the background tasks scheduled through the native interface. This class acts as a
 * proxy, loads native, creates the task and forwards the method calls.
 */
@NullMarked
public final class ProxyNativeTask extends NativeBackgroundTask {
    @JniType("::ProxyNativeTask")
    interface NativeProxyNativeTask extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeProxyNativeTask> mNativeProxyNativeTask;

    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        String extras = taskParameters.getExtras().getString(TaskInfo.SERIALIZED_TASK_EXTRAS, "");
        Callback<Boolean> wrappedCallback =
                needsReschedule -> {
                    callback.taskFinished(needsReschedule);
                    destroy();
                };

        mNativeProxyNativeTask =
                ProxyNativeTaskJni.get()
                        .init(this, taskParameters.getTaskId(), extras, wrappedCallback);

        boolean isFullBrowserStarted =
                BrowserStartupController.getInstance().isFullBrowserStarted();
        if (isFullBrowserStarted) {
            ProxyNativeTaskJni.get()
                    .startBackgroundTaskWithFullBrowser(
                            mNativeProxyNativeTask, ProfileManager.getLastUsedRegularProfile());
        } else {
            ProxyNativeTaskJni.get()
                    .startBackgroundTaskInReducedMode(
                            mNativeProxyNativeTask, ProfileKeyUtil.getLastUsedRegularProfileKey());
            BrowserStartupController.getInstance()
                    .addStartupCompletedObserver(
                            new BrowserStartupController.StartupCallback() {
                                @Override
                                public void onSuccess(@Nullable StartupMetrics metrics) {
                                    if (mNativeProxyNativeTask == null) return;
                                    ProxyNativeTaskJni.get()
                                            .onFullBrowserLoaded(
                                                    mNativeProxyNativeTask,
                                                    ProfileManager.getLastUsedRegularProfile());
                                }

                                @Override
                                public void onFailure() {}
                            });
        }
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        if (mNativeProxyNativeTask == null) return false;
        boolean taskNeedsReschedule =
                ProxyNativeTaskJni.get().stopBackgroundTask(mNativeProxyNativeTask);
        destroy();
        return taskNeedsReschedule;
    }

    @Override
    protected boolean supportsMinimalBrowser() {
        // Return true here if you want your task to be run in reduced mode.
        return false;
    }

    private void destroy() {
        if (mNativeProxyNativeTask == null) return;
        mNativeProxyNativeTask.destroy();
        mNativeProxyNativeTask = null;
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeProxyNativeTask> init(
                ProxyNativeTask self,
                int taskType,
                @JniType("std::string") String extras,
                Callback<Boolean> callback);

        void startBackgroundTaskInReducedMode(JniPtr<NativeProxyNativeTask> self, ProfileKey key);

        void startBackgroundTaskWithFullBrowser(
                JniPtr<NativeProxyNativeTask> self, @JniType("Profile*") Profile profile);

        void onFullBrowserLoaded(
                JniPtr<NativeProxyNativeTask> self, @JniType("Profile*") Profile profile);

        boolean stopBackgroundTask(JniPtr<NativeProxyNativeTask> self);
    }
}
