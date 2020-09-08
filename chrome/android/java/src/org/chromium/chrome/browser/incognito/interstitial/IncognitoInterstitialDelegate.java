// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.app.Activity;

import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;

/**
 * This class is responsible for the providing the functionality to the "Learn More" and "Continue"
 * button in the Incognito interstitial.
 */
public class IncognitoInterstitialDelegate {
    private final Activity mActivity;
    private final TabCreator mIncognitoTabCreator;
    private final HelpAndFeedback mHelpAndFeedback;
    private final String mCurrentUrl;

    public IncognitoInterstitialDelegate(Activity activity, TabCreator incognitoTabCreator,
            HelpAndFeedback helpAndFeedback, String currentUrl) {
        mActivity = activity;
        mIncognitoTabCreator = incognitoTabCreator;
        mHelpAndFeedback = helpAndFeedback;
        mCurrentUrl = currentUrl;
    }

    /**
     * Open the help centre article regarding Incognito usage.
     */
    @MainThread
    public void openLearnMorePage() {
        ThreadUtils.assertOnUiThread();
        mHelpAndFeedback.show(mActivity,
                mActivity.getString(R.string.help_context_incognito_learn_more),
                Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(), null);
    }

    /**
     * Navigates to |mCurrentUrl| in a new incognito tab.
     */
    @MainThread
    public void openCurrentUrlInIncognitoTab() {
        // TODO(https://crbug.com/1120334): Add metrics to web sign-in flow.
        ThreadUtils.assertOnUiThread();
        mIncognitoTabCreator.launchUrl(mCurrentUrl, TabLaunchType.FROM_CHROME_UI);
    }
}