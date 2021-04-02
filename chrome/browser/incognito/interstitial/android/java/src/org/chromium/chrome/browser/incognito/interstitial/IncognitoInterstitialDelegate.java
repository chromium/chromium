// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.app.Activity;

import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

/**
 * This class is responsible for the providing the functionality to the "Learn More" and "Continue"
 * button in the Incognito interstitial. This should be used in conjunction with the incognito
 * interstitial MVC.
 */
public class IncognitoInterstitialDelegate {
    private final Activity mActivity;
    private final TabCreator mIncognitoTabCreator;
    private final HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    private final TabModel mRegularTabModel;

    /**
     * @param activity The {@link Activity} which would be used to show the HelpAndFeedback page.
     * @param regularTabModel A regular {@link TabModel} via which we would close the
     *         current regular tab in which the interstitial is shown after the user clicked on
     *         "Continue" in the incognito interstitial.
     * @param incognitoTabCreator An incognito {@link TabCreator} instance which would be used to
     *         create incognito tab after the user clicks on "Continue" in the incognito
     *         interstitial.
     * @param helpAndFeedbackLauncher A {@link HelpAndFeedbackLauncher} instance through which we
     *         will load the
     */
    public IncognitoInterstitialDelegate(Activity activity, TabModel regularTabModel,
            TabCreator incognitoTabCreator, HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mActivity = activity;
        mRegularTabModel = regularTabModel;
        mIncognitoTabCreator = incognitoTabCreator;
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
    }

    /**
     * Open the help centre article regarding Incognito usage.
     */
    @MainThread
    public void openLearnMorePage() {
        ThreadUtils.assertOnUiThread();
        mHelpAndFeedbackLauncher.show(mActivity,
                mActivity.getString(R.string.help_context_incognito_learn_more),
                Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(/*createIfNeeded=*/true),
                null);
    }

    /**
     * Navigates to the URL currently shown in the regular tab, in a new incognito tab and closes
     * the current regular tab.
     */
    @MainThread
    public void openCurrentUrlInIncognitoTab() {
        ThreadUtils.assertOnUiThread();
        Tab currentRegularTab = TabModelUtils.getCurrentTab(mRegularTabModel);
        mIncognitoTabCreator.launchUrl(
                currentRegularTab.getUrlString(), TabLaunchType.FROM_CHROME_UI);
        mRegularTabModel.closeTab(currentRegularTab);
    }
}
