// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import java.util.Map;

/**
 * Class that controls retrieving and displaying surveys. Clients should call #downloadSurvey() and
 * register a runnable to run when the survey is available. After downloading the survey, call
 * {@link showSurveyIfAvailable()} to display the survey.
 */
public class SurveyController {
    private static SurveyController sTestInstance;

    /**
     * @return The SurveyController to use during the lifetime of the browser process.
     */
    public static SurveyController create() {
        if (sTestInstance != null) {
            return sTestInstance;
        }
        return AppHooks.get().createSurveyController();
    }

    /** Set the instance to use for survey related tests. Reset back to null after tests. */
    public static void setInstanceForTesting(SurveyController testInstance) {
        sTestInstance = testInstance;
        ResettersForTesting.register(() -> sTestInstance = null);
    }

    /**
     * Asynchronously downloads the survey using the provided parameters.
     * @param context The context used to create the survey.
     * @param triggerId  The ID used to fetch the data for the surveys.
     * @param onSuccessRunnable The runnable to notify when the survey is ready.
     * @param onFailureRunnable The runnable to notify when downloading the survey failed, or the
     *                          survey does not exist.
     */
    protected void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
            Runnable onFailureRunnable) {}

    /**
     * Show the survey.
     * @param activity The client activity for the survey request.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param showAsBottomSheet Whether the survey should be presented as a bottom sheet or not.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param lifecycleDispatcher LifecycleDispatcher that will dispatch different activity signals.
     *
     * @deprecated Use {@link #showSurveyIfAvailable(Activity, String, int,
     *         ActivityLifecycleDispatcher, Map)} instead.
     */
    @Deprecated
    protected void showSurveyIfAvailable(Activity activity, String siteId,
            boolean showAsBottomSheet, int displayLogoResId,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {}

    /**
     * Show the survey.
     * @param activity The client activity for the survey request.
     * @param triggerId Id used to trigger the survey.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param lifecycleDispatcher LifecycleDispatcher that will dispatch different activity signals.
     * @param psd key-value set of list of PSD attaching to the survey.
     */
    protected void showSurveyIfAvailable(Activity activity, String triggerId, int displayLogoResId,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            @Nullable Map<String, String> psd) {
        throw new UnsupportedOperationException();
    }

    /**
     * Check if a survey is valid or expired.
     * @param triggerId  The ID used to fetch the data for the surveys.
     * @return true if the survey has expired, false if the survey is valid.
     */
    protected boolean isSurveyExpired(String triggerId) {
        return false;
    }
}
