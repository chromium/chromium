// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.help;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.feedback.FeedbackCollector;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;

import javax.annotation.Nonnull;

/**
 * Launches an activity that displays a relevant support page and has an option to provide feedback.
 */
public class HelpAndFeedback {
    protected static final String FALLBACK_SUPPORT_URL =
            "https://support.google.com/chrome/topic/6069782";
    private static final String TAG = "HelpAndFeedback";

    private static HelpAndFeedback sInstance;

    /**
     * Returns the singleton instance of HelpAndFeedback, creating it if needed.
     */
    public static HelpAndFeedback getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createHelpAndFeedback();
        }
        return sInstance;
    }

    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a
     *                 screenshot of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *                    context and will be used to show a more relevant help page.
     * @param collector the {@link FeedbackCollector} to use for extra data. Must not be null.
     */
    protected void show(
            Activity activity, String helpContext, @Nonnull FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        launchFallbackSupportUri(activity);
    }

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param collector the {@link FeedbackCollector} to use for extra data. Must not be null.
     */
    protected void showFeedback(Activity activity, @Nonnull FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        launchFallbackSupportUri(activity);
    }

    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a
     *                 screenshot of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *                    context and will be used to show a more relevant help page.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     */
    public void show(final Activity activity, final String helpContext, Profile profile,
            @Nullable String url) {
        RecordUserAction.record("MobileHelpAndFeedback");
        new FeedbackCollector(activity, profile, url, null /* categoryTag */,
                null /* description */, helpContext, true /* takeScreenshot */,
                collector -> show(activity, helpContext, collector));
    }

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     */
    public void showFeedback(final Activity activity, Profile profile, @Nullable String url,
            @Nullable final String categoryTag) {
        new FeedbackCollector(activity, profile, url, categoryTag, null /* description */, null,
                true /* takeScreenshot */, collector -> showFeedback(activity, collector));
    }

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     * @param feedbackContext The context that describes the current feature being used.
     */
    public void showFeedback(final Activity activity, Profile profile, @Nullable String url,
            @Nullable final String categoryTag, @Nullable final String feedbackContext) {
        new FeedbackCollector(activity, profile, url, categoryTag, null /* description */,
                feedbackContext, true /* takeScreenshot */,
                collector -> showFeedback(activity, collector));
    }

    /**
     * Get help context ID from URL.
     *
     * @param url The URL to be checked.
     * @param isIncognito Whether we are in incognito mode or not.
     * @return Help context ID that matches the URL and incognito mode.
     */
    public static String getHelpContextIdFromUrl(Context context, String url, boolean isIncognito) {
        if (TextUtils.isEmpty(url)) {
            return context.getString(R.string.help_context_general);
        } else if (url.startsWith(UrlConstants.BOOKMARKS_URL)) {
            return context.getString(R.string.help_context_bookmarks);
        } else if (url.equals(UrlConstants.HISTORY_URL)) {
            return context.getString(R.string.help_context_history);
        // Note: For www.google.com the following function returns false.
        } else if (UrlUtilitiesJni.get().isGoogleSearchUrl(url)) {
            return context.getString(R.string.help_context_search_results);
        // For incognito NTP, we want to show incognito help.
        } else if (isIncognito) {
            return context.getString(R.string.help_context_incognito);
        } else if (url.equals(UrlConstants.NTP_URL)) {
            return context.getString(R.string.help_context_new_tab);
        }
        return context.getString(R.string.help_context_webpage);
    }

    protected static void launchFallbackSupportUri(Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(FALLBACK_SUPPORT_URL));
        // Let Chrome know that this intent is from Chrome, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(context.getPackageName());
        context.startActivity(intent);
    }
}
