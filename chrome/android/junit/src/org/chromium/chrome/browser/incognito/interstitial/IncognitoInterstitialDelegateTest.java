// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;

/**
 * Roboelectric tests class for the incognito interstitial.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoInterstitialDelegateTest {
    private static final String sIncognitoLearnMoreText = "dummy_chrome_incognito";
    private static final String sContinueUrlPage = "dummy_url_string.com";

    @Mock
    private HelpAndFeedback mHelpAndFeedbackMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private Activity mActivityMock;

    @Mock
    private TabCreator mIncognitoTabCreatorMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mActivityMock.getString(R.string.help_context_incognito_learn_more))
                .thenReturn(sIncognitoLearnMoreText);

        Profile.setLastUsedProfileForTesting(mProfileMock);
        mIncognitoInterstitialDelegate = new IncognitoInterstitialDelegate(
                mActivityMock, mIncognitoTabCreatorMock, mHelpAndFeedbackMock, sContinueUrlPage);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    private IncognitoInterstitialDelegate mIncognitoInterstitialDelegate;

    @Test
    @MediumTest
    public void testOpenLearnMorePage() {
        mIncognitoInterstitialDelegate.openLearnMorePage();
        verify(mHelpAndFeedbackMock)
                .show(mActivityMock, sIncognitoLearnMoreText, mProfileMock.getPrimaryOTRProfile(),
                        null);
    }

    @Test
    @MediumTest
    public void testOpenCurrentUrlInIncognitoTab() {
        mIncognitoInterstitialDelegate.openCurrentUrlInIncognitoTab();
        verify(mIncognitoTabCreatorMock).launchUrl(sContinueUrlPage, TabLaunchType.FROM_CHROME_UI);
    }
}
