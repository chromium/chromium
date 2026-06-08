// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Delegate that handles the display of the HelpAndFeedback flows. */
@NullMarked
public interface HelpAndFeedbackLauncherDelegate {
    String FALLBACK_SUPPORT_URL = "https://support.google.com/chrome/topic/6069782";
    String P_LINK_URL_PREFIX = "https://support.google.com/chrome/?p=";

    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a screenshot
     *     of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *     context and will be used to show a more relevant help page.
     * @param collector the {@link FeedbackCollector} to use for extra data. Must not be null.
     */
    void show(Activity activity, String helpContext, FeedbackCollector collector);

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *     screenshot of.
     * @param collector the {@link FeedbackCollector} to use for extra data. Must not be null.
     */
    void showFeedback(Activity activity, FeedbackCollector collector);

    /**
     * Generates a p-link help URL for a given help context string.
     *
     * @param helpContext The help context string.
     * @return The generated help URL.
     */
    static String getPLinkHelpUrl(String helpContext) {
        return P_LINK_URL_PREFIX + helpContext;
    }

    // TODO(crbug.com/365755043): Remove once downstream call has migrated.
    static void launchFallbackSupportUri(Context context) {
        launchFallbackSupportUri(context, null);
    }

    /**
     * Handles the fallback help case of opening the URL in the browser.
     *
     * @param context The context launching the fallback support.
     * @param url The url to open.
     */
    static void launchFallbackSupportUri(Context context, @Nullable String url) {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(TextUtils.isEmpty(url) ? FALLBACK_SUPPORT_URL : url));
        // Let Chrome know that this intent is from Chrome, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(context.getPackageName());
        context.startActivity(intent);
    }
}
