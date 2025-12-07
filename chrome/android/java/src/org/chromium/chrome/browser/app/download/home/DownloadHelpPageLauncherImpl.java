// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.download.home.DownloadHelpPageLauncher;
import org.chromium.chrome.browser.profiles.Profile;

/** Implementation for {@link DownloadHelpPageLauncher}. */
@NullMarked
class DownloadHelpPageLauncherImpl implements DownloadHelpPageLauncher {
    // Whether help pages should be opened in an Incognito CCT.
    private final boolean mIsOffTheRecord;

    public DownloadHelpPageLauncherImpl(Profile profile) {
        mIsOffTheRecord = profile.isOffTheRecord();
    }

    /** Opens the given URL in a CCT, respecting whether the profile is OTR. */
    @Override
    public void openUrl(final Context context, String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (mIsOffTheRecord) {
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    intent, BrowserServicesIntentDataProvider.IncognitoCctCallerId.DOWNLOAD_HOME);
        }
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(context, intent);
    }
}
