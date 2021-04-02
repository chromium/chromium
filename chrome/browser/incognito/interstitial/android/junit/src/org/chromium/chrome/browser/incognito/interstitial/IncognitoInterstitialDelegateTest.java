// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * Roboelectric tests class for the incognito interstitial.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoInterstitialDelegateTest {
    private static final String sIncognitoLearnMoreText = "dummy_chrome_incognito";
    private static final String sCurrentUrlPage = "dummy_url_string.com";

    @Mock
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncherMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private Activity mActivityMock;

    @Mock
    private TabCreator mIncognitoTabCreatorMock;

    @Mock
    private TabModel mRegularTabModelMock;

    @Mock
    private Tab mTabMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mActivityMock.getString(R.string.help_context_incognito_learn_more))
                .thenReturn(sIncognitoLearnMoreText);
        when(mRegularTabModelMock.getTabAt(anyInt())).thenReturn(mTabMock);
        when(mRegularTabModelMock.closeTab(mTabMock)).thenReturn(true);
        when(mTabMock.getUrlString()).thenReturn(sCurrentUrlPage);

        Profile.setLastUsedProfileForTesting(mProfileMock);
        mIncognitoInterstitialDelegate = new IncognitoInterstitialDelegate(mActivityMock,
                mRegularTabModelMock, mIncognitoTabCreatorMock, mHelpAndFeedbackLauncherMock);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    private IncognitoInterstitialDelegate mIncognitoInterstitialDelegate;

    @Test
    public void testOpenLearnMorePage() {
        mIncognitoInterstitialDelegate.openLearnMorePage();
        verify(mHelpAndFeedbackLauncherMock)
                .show(mActivityMock, sIncognitoLearnMoreText,
                        mProfileMock.getPrimaryOTRProfile(/*createIfNeeded=*/true), null);
    }

    @Test
    public void testOpenCurrentUrlInIncognitoTab() {
        mIncognitoInterstitialDelegate.openCurrentUrlInIncognitoTab();
        verify(mIncognitoTabCreatorMock).launchUrl(sCurrentUrlPage, TabLaunchType.FROM_CHROME_UI);
        verify(mRegularTabModelMock).closeTab(mTabMock);
    }
}
