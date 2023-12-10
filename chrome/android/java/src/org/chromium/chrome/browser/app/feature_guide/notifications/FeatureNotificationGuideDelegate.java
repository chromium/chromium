// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feature_guide.notifications;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationGuideService;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationUtils;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureType;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Provides chrome layer dependencies required for {@link FeatureNotificationGuideService}. */
public final class FeatureNotificationGuideDelegate
        implements FeatureNotificationGuideService.Delegate {
    /**
     * Launches an activity to show IPH when a feature notification is clicked.
     * @param featureType The type of the feature being promoed in the notification.
     */
    @Override
    public void launchActivityToShowIph(@FeatureType int featureType) {
        ThreadUtils.assertOnUiThread();
        boolean shouldOpenNewTab =
                featureType == FeatureType.INCOGNITO_TAB
                        || featureType == FeatureType.VOICE_SEARCH
                        || featureType == FeatureType.NTP_SUGGESTION_CARD;
        Class activityToLaunch = getActivityToLaunchForFeature(featureType);
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, activityToLaunch);
        if (shouldOpenNewTab) {
            intent.setData(Uri.parse(UrlConstants.NTP_URL));
            intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(FeatureNotificationUtils.EXTRA_FEATURE_TYPE, featureType);
        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private Class getActivityToLaunchForFeature(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.INCOGNITO_TAB:
            case FeatureType.NTP_SUGGESTION_CARD:
            case FeatureType.VOICE_SEARCH:
            case FeatureType.DEFAULT_BROWSER:
                return ChromeTabbedActivity.class;
            case FeatureType.SIGN_IN:
                return SettingsActivity.class;
            default:
                assert false : "Unexpected feature type " + featureType;
                return ChromeTabbedActivity.class;
        }
    }
}
