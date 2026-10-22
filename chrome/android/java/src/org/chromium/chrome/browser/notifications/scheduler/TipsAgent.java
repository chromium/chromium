// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tips.TipsNotificationsFeatureType;

/**
 * Used by tips notifications to display tips through the Android UI. Scheduling lives in {@link
 * org.chromium.chrome.browser.tips.TipsUtils}.
 *
 * <p>TODO(crbug.com/568415688): Move this into //chrome/browser/tips/android once the tips intent
 * extra and the trusted new tab intent helper are available outside of {@link IntentHandler}.
 */
@NullMarked
public class TipsAgent {
    @CalledByNative
    private static void showTipsPromo(@TipsNotificationsFeatureType int featureType) {
        Context context = ContextUtils.getApplicationContext();
        Intent newIntent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        // Use an existing NTP if available to show the feature promo on, tied into the associated
        // handling switch case statement in ChromeTabbedActivity.java.
        newIntent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.setTabLaunchType(newIntent, TabLaunchType.FROM_TIPS_NOTIFICATIONS);
        context.startActivity(newIntent);
    }

    private TipsAgent() {}
}
