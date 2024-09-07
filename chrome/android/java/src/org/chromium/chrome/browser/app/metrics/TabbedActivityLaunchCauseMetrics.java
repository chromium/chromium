// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.metrics;

import android.app.Activity;
import android.content.Intent;
import android.nfc.NfcAdapter;
import android.speech.RecognizerResultsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.webapps.ShortcutSource;

/** LaunchCauseMetrics for ChromeTabbedActivity. */
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
                        launchIntent, WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN)) {
            return LaunchCause.HOME_SCREEN_WIDGET;
        }

        if (ShortcutSource.ADD_TO_HOMESCREEN_SHORTCUT
                == IntentUtils.safeGetIntExtra(
                        launchIntent, WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN)) {
            return LaunchCause.HOME_SCREEN_SHORTCUT;
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

        if (isNotificationLaunch(launchIntent)) return LaunchCause.NOTIFICATION;

        if (IntentHandler.BringToFrontSource.SEARCH_ACTIVITY
                == getBringTabToFrontSource(launchIntent)) {
            return LaunchCause.HOME_SCREEN_WIDGET;
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

        if (Intent.ACTION_SEND.equals(launchIntent.getAction())) {
            return LaunchCause.SHARE_INTENT;
        }

        boolean isExternalIntentFromChrome =
                IntentHandler.isExternalIntentSourceChrome(launchIntent);
        if (Intent.ACTION_VIEW.equals(launchIntent.getAction()) && !isExternalIntentFromChrome) {
            return LaunchCause.EXTERNAL_VIEW_INTENT;
        }

        if (NfcAdapter.ACTION_NDEF_DISCOVERED.equals(launchIntent.getAction())) {
            return LaunchCause.NFC;
        }

        if (isExternalIntentFromChrome) return LaunchCause.OTHER_CHROME;
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

        // Notifications should still count as an intentional transition when moving between
        // ChromeActivitys.
        if (isNotificationLaunch(launchIntent)) return LaunchCause.NOTIFICATION;

        return LaunchCause.OTHER;
    }

    private boolean isNotificationLaunch(Intent intent) {
        // ServiceWorker notification to open a new window.
        if (intent.hasExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA)) {
            return true;
        }

        // For now, just assume that a source of ACTIVATE_TAB counts as a notification, as there are
        // many reasons why a tab/webcontents might get Activated by Chrome (and plumbing all
        // sources of tab activation is impractical), but the only ones that should be triggerable
        // while Chrome is in the background (outside of tests) are from media/ServiceWorker
        // notifications.
        @IntentHandler.BringToFrontSource int source = getBringTabToFrontSource(intent);
        if (IntentHandler.BringToFrontSource.NOTIFICATION == source
                || IntentHandler.BringToFrontSource.ACTIVATE_TAB == source) {
            return true;
        }
        return false;
    }

    // Returns the source of the BringTabToFront intent, or null if it's not a BringTabToFront
    // intent.
    private int getBringTabToFrontSource(Intent intent) {
        if (IntentHandler.getBringTabToFrontId(intent) == Tab.INVALID_TAB_ID) {
            return IntentHandler.BringToFrontSource.INVALID;
        }
        return IntentUtils.safeGetIntExtra(
                intent,
                IntentHandler.BRING_TAB_TO_FRONT_SOURCE_EXTRA,
                IntentHandler.BringToFrontSource.INVALID);
    }
}
