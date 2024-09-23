// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;

/**
 * Used by the various password access loss warning surfaces to show p-link based help center
 * articles.
 *
 * <p>TODO(crbug.com/365755043): Replace this with HelpAndFeedbackLauncher once it starts supporting
 * p-links.
 */
public class HelpUrlLauncher {
    static final String KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL =
            "https://support.google.com/googleplay/?p=keep_apps_and_devices_working_with_gms";
    static final String GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL =
            "https://support.google.com/googleplay/?p=google_play_supported_devices";
    private final CustomTabIntentHelper mCustomTabIntentHelper;

    HelpUrlLauncher(CustomTabIntentHelper customTabIntentHelper) {
        mCustomTabIntentHelper = customTabIntentHelper;
    }

    void showHelpArticle(Activity activity, String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        activity, customTabIntent.intent);
        intent.setPackage(activity.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(activity, intent);
    }
}
