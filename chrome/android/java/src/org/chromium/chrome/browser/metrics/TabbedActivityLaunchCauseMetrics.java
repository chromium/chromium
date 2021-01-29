// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.content.Intent;
import android.speech.RecognizerResultsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.components.webapps.ShortcutSource;

/**
 * LaunchCauseMetrics for ChromeTabbedActivity.
 */
public class TabbedActivityLaunchCauseMetrics extends LaunchCauseMetrics {
    private final Activity mActivity;

    public TabbedActivityLaunchCauseMetrics(Activity activity) {
        super(activity);
        mActivity = activity;
    }

    @Override
    protected @LaunchCause int computeIntentLaunchCause() {
        Intent launchIntent = mActivity.getIntent();
        if (launchIntent == null) return LaunchCause.OTHER;

        if (IntentUtils.isMainIntentFromLauncher(launchIntent)) {
            return LaunchCause.MAIN_LAUNCHER_ICON;
        }

        if (IntentUtils.safeGetBooleanExtra(
                    launchIntent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false)
                && IntentHandler.wasIntentSenderChrome(launchIntent)) {
            return LaunchCause.MAIN_LAUNCHER_ICON_SHORTCUT;
        }

        if (ShortcutSource.BOOKMARK_NAVIGATOR_WIDGET
                == IntentUtils.safeGetIntExtra(
                        launchIntent, ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN)) {
            return LaunchCause.HOME_SCREEN_WIDGET;
        }

        if (IntentUtils.safeGetBooleanExtra(
                    launchIntent, SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false)) {
            if (IntentUtils.safeGetBooleanExtra(
                        launchIntent, SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, false)) {
                return LaunchCause.HOME_SCREEN_WIDGET;
            }
            // Intent came through the Search Activity but wasn't from the Search Widget, so
            // probably came from LaunchIntentDispatcher#processWebSearchIntent, and no other
            // installed apps could handle web search.
            return LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT;
        }

        if (RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS.equals(launchIntent.getAction())) {
            return LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT;
        }

        // This is unlikely to be hit here (much more likely to see Open In Browser intents in
        // getIntentionalTransitionCauseOrOther() below), but an Intent Picker dialog could cause
        // Chrome to be backgrounded on some Android distributions, or on tiny screens. This will
        // also be hit when Open In Browser crosses Chrome channels
        // (eg. Chrome Stable CCT -> Open In Browser -> User chooses Canary)
        if (IntentUtils.safeGetBooleanExtra(
                    launchIntent, IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, false)) {
            return LaunchCause.OPEN_IN_BROWSER_FROM_MENU;
        }

        // TODO(https://crbug.com/1163961): Implement remaining ChromeTabbedActivity launch cause
        // metrics.

        return LaunchCause.OTHER;
    }

    @Override
    protected @LaunchCause int getIntentionalTransitionCauseOrOther() {
        Intent launchIntent = mActivity.getIntent();
        if (!didReceiveIntent() || launchIntent == null) return LaunchCause.OTHER;

        if (IntentUtils.safeGetBooleanExtra(
                    launchIntent, IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, false)) {
            return LaunchCause.OPEN_IN_BROWSER_FROM_MENU;
        }

        return LaunchCause.OTHER;
    }
}
