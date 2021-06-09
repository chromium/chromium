// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * Class that controls retrieving and displaying surveys. Clients should call #downloadSurvey() and
 * register a runnable to run when the survey is available. After downloading the survey, call
 * {@link showSurveyIfAvailable()} to display the survey.
 */
public class SurveyController {
    private static SurveyController sInstance;

    /**
     * @return The SurveyController to use during the lifetime of the browser process.
     */
    public static SurveyController getInstance() {
        if (sInstance == null) {
            sInstance = AppHooks.get().createSurveyController();
        }
        return sInstance;
    }

    @VisibleForTesting
    static void setInstanceForTesting(SurveyController testInstance) {
        sInstance = testInstance;
    }

    /**
     * Returns if there already exists a downloaded survey from the provided site id.
     * @deprecated Use {@link #downloadSurvey(Context, String, Runnable, Runnable)} instead.
     * @param siteId The id of the site from where the survey should have been downloaded.
     * @param context The context of the application.
     * @return If the survey with a matching site id exists.
     */
    @Deprecated
    public boolean doesSurveyExist(String siteId, Context context) {
        return false;
    }

    /**
     * Asynchronously downloads the survey using the provided parameters.
     * @deprecated Replaced with {@link #downloadSurvey(Context, String, Runnable, Runnable)}.
     * @param context The context used to register a broadcast receiver.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param onSuccessRunnable The runnable to notify when the survey is ready.
     *                          If no survey is available, the runnable will not be run.
     * @param siteContext Optional parameter to build the download request. Site context can be
     *                    used for adding metadata.
     */
    @Deprecated
    public void downloadSurvey(
            Context context, String siteId, Runnable onSuccessRunnable, String siteContext) {}

    /**
     * Asynchronously downloads the survey using the provided parameters.
     * @param context The context used to create the survey.
     * @param triggerId  The ID used to fetch the data for the surveys.
     * @param onSuccessRunnable The runnable to notify when the survey is ready.
     * @param onFailureRunnable The runnable to notify when downloading the survey failed, or the
     *                          survey does not exist.
     */
    public void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
            Runnable onFailureRunnable) {}

    /**
     * @deprecated Use #showSurveyIfAvailable(Activity, String, boolean, int,
     *         ActivityLifecycleDispatcher).
     */
    @Deprecated
    public void showSurveyIfAvailable(
            Activity activity, String siteId, boolean showAsBottomSheet, int displayLogoResId) {}

    /**
     * Show the survey.
     * @param activity The client activity for the survey request.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param showAsBottomSheet Whether the survey should be presented as a bottom sheet or not.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param lifecycleDispatcher LifecycleDispatcher that will dispatch different activity signals.
     */
    public void showSurveyIfAvailable(Activity activity, String siteId, boolean showAsBottomSheet,
            int displayLogoResId, @Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
        this.showSurveyIfAvailable(activity, siteId, showAsBottomSheet, displayLogoResId);
    }

    /**
     * Clears the survey cache containing responses and history.
     * @param context The context used to clear the cache.
     */
    public void clearCache(Context context) {}
}
