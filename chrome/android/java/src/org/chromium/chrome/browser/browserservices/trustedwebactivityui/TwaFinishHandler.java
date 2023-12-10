// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.webapps.WebApkServiceClient;

import javax.inject.Inject;

/** Applies TWA-specific logic when the activity is about to finish. */
@ActivityScope
public class TwaFinishHandler {
    private static final String FINISH_TASK_COMMAND_NAME = "finishAndRemoveTask";
    private static final String SUCCESS_KEY = "success";

    private final Activity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;

    private boolean mShouldAttemptFinishingTask;

    @Inject
    public TwaFinishHandler(
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection connection) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
    }

    public void setShouldAttemptFinishingTask(boolean shouldAttemptFinishingTask) {
        mShouldAttemptFinishingTask = shouldAttemptFinishingTask;
    }

    /**
     * Called when activity is about to finish.
     * @param defaultFinishTaskRunnable Runnable that will be called if default behavior should be
     * applied.
     */
    public void onFinish(Runnable defaultFinishTaskRunnable) {
        if (!mShouldAttemptFinishingTask) {
            defaultFinishTaskRunnable.run();
            return;
        }
        boolean success = finishAndRemoveTask();
        if (!success) {
            // E.g. the TWA/WebApk launcher activity is not on the bottom of the stack.
            defaultFinishTaskRunnable.run();
        }
    }

    private boolean finishAndRemoveTask() {
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        if (webApkExtras != null && Build.VERSION.SDK_INT >= 23) {
            WebApkServiceClient.getInstance().finishAndRemoveTaskSdk23(mActivity, webApkExtras);
            return true;
        }

        // This is the analogue to IWebApkApi#finishAndRemoveTaskSdk23().
        // Currently we don't make this API public, because there could potentially be a way of
        // avoiding it altogether in the two use cases WebAPKs currently have.
        Bundle result =
                mConnection.sendExtraCallbackWithResult(
                        mIntentDataProvider.getSession(), FINISH_TASK_COMMAND_NAME, null);
        return result != null && result.getBoolean(SUCCESS_KEY, false);
    }
}
