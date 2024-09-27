// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;

import java.util.Map;

/**
 * Launches an activity that displays a relevant support page and has an option to provide feedback.
 */
public class HelpAndFeedbackLauncherImpl implements HelpAndFeedbackLauncher {
    protected static final String FALLBACK_SUPPORT_URL =
            "https://support.google.com/chrome/topic/6069782";

    private static ProfileKeyedMap<HelpAndFeedbackLauncher> sProfileToLauncherMap;
    private final HelpAndFeedbackLauncherDelegate mDelegate;
    private final Profile mProfile;

    /**
     * @return The HelpAndFeedbackLauncher for a given profile, creating it if needed.
     */
    public static HelpAndFeedbackLauncher getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (sProfileToLauncherMap == null) {
            sProfileToLauncherMap =
                    new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileToLauncherMap.getForProfile(profile, HelpAndFeedbackLauncherImpl::new);
    }

    private HelpAndFeedbackLauncherImpl(Profile profile) {
        mProfile = profile;

        HelpAndFeedbackLauncherDelegate delegate =
                ServiceLoaderUtil.maybeCreate(HelpAndFeedbackLauncherDelegate.class);
        if (delegate == null) {
            delegate = new FallbackHelpAndFeedbackLauncherDelegate();
        }
        mDelegate = delegate;
    }

    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a screenshot
     *     of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *     context and will be used to show a more relevant help page.
     * @param url the current URL. May be null.
     */
    @Override
    public void show(final Activity activity, final String helpContext, @Nullable String url) {
        RecordUserAction.record("MobileHelpAndFeedback");
        new ChromeFeedbackCollector(
                activity,
                /* categoryTag= */ null,
                /* description= */ null,
                new ScreenshotTask(activity),
                new ChromeFeedbackCollector.InitParams(mProfile, url, helpContext),
                collector -> mDelegate.show(activity, helpContext, collector),
                mProfile);
    }

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     * @param screenshotMode The kind of screenshot to include with the feedback.
     * @param feedbackContext The context that describes the current feature being used.
     */
    @Override
    public void showFeedback(
            final Activity activity,
            @Nullable String url,
            @Nullable final String categoryTag,
            @ScreenshotMode int screenshotMode,
            @Nullable final String feedbackContext) {
        long startTime = SystemClock.elapsedRealtime();
        new ChromeFeedbackCollector(
                activity,
                categoryTag,
                /* description= */ null,
                new ScreenshotTask(activity, screenshotMode),
                new ChromeFeedbackCollector.InitParams(mProfile, url, feedbackContext),
                (collector) -> {
                    RecordHistogram.recordLongTimesHistogram(
                            "Feedback.Duration.FormOpenToSubmit",
                            SystemClock.elapsedRealtime() - startTime);
                    mDelegate.showFeedback(activity, collector);
                },
                mProfile);
    }

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     */
    @Override
    public void showFeedback(
            final Activity activity, @Nullable String url, @Nullable final String categoryTag) {
        showFeedback(activity, url, categoryTag, ScreenshotMode.DEFAULT, null);
    }

    /**
     * Starts an activity prompting the user to enter feedback for the interest feed.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param categoryTag The category that this feedback report falls under.
     * @param feedContext Feed specific parameters (url, title, etc) to include with feedback.
     */
    @Override
    public void showFeedback(
            final Activity activity,
            @Nullable String url,
            @Nullable final String categoryTag,
            @Nullable final Map<String, String> feedContext) {
        new FeedFeedbackCollector(
                activity,
                categoryTag,
                /* description= */ null,
                new ScreenshotTask(activity),
                new FeedFeedbackCollector.InitParams(mProfile, url, feedContext),
                collector -> mDelegate.showFeedback(activity, collector),
                mProfile);
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
        }
        // Note: For www.google.com the following function returns false.
        else if (UrlUtilitiesJni.get().isGoogleSearchUrl(url)) {
            return context.getString(R.string.help_context_search_results);
        }
        // For incognito NTP, we want to show incognito help.
        else if (isIncognito) {
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
