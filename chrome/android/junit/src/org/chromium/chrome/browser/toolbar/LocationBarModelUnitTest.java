// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for the LocationBarModel.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarModelUnitTest {
    @Mock
    private Tab mIncognitoTabMock;

    @Mock
    private Tab mRegularTabMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private CustomTabIncognitoManager mCustomTabIncognitoManagerMock;

    @Mock
    private Profile mRegularProfileMock;

    @Mock
    private Profile mPrimaryOTRProfileMock;

    @Mock
    private Profile mNonPrimaryOTRProfileMock;
    @Mock
    private LocationBarDataProvider.Observer mLocationBarDataObserver;
    @Mock
    private SearchEngineLogoUtils mSearchEngineLogoUtils;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mRegularProfileMock);
        CustomTabIncognitoManager.setCustomTabIncognitoManagerUsedForTesting(
                mCustomTabIncognitoManagerMock);

        when(mCustomTabIncognitoManagerMock.getProfile()).thenReturn(mNonPrimaryOTRProfileMock);
        when(mRegularProfileMock.hasPrimaryOTRProfile()).thenReturn(true);
        when(mRegularProfileMock.getPrimaryOTRProfile(/*createIfNeeded=*/true))
                .thenReturn(mPrimaryOTRProfileMock);
        when(mIncognitoTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        when(mIncognitoTabMock.isIncognito()).thenReturn(true);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        CustomTabIncognitoManager.setCustomTabIncognitoManagerUsedForTesting(null);
    }

    public static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(WebContents webContents) {
                    return true;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return true;
                }
            };

    // clang-format off
    private static class TestIncognitoLocationBarModel extends LocationBarModel {
        public TestIncognitoLocationBarModel(Tab tab, SearchEngineLogoUtils searchEngineLogoUtils) {
            super(ContextUtils.getApplicationContext(), NewTabPageDelegate.EMPTY,
                    url -> url.getSpec(), IncognitoUtils::getNonPrimaryOTRProfileFromWindowAndroid,
                    OFFLINE_STATUS, searchEngineLogoUtils);
            setTab(tab, /*incognito=*/true);
        }
    }

    private static class TestRegularLocationBarModel extends LocationBarModel {
        public TestRegularLocationBarModel(Tab tab, SearchEngineLogoUtils searchEngineLogoUtils) {
            super(ContextUtils.getApplicationContext(), NewTabPageDelegate.EMPTY,
                    url -> url.getSpec(), IncognitoUtils::getNonPrimaryOTRProfileFromWindowAndroid,
                    OFFLINE_STATUS, searchEngineLogoUtils);
            setTab(tab, /*incognito=*/false);
        }
    }
    // clang-format on

    @Test
    @MediumTest
    public void getProfile_IncognitoTab_ReturnsPrimaryOTRProfile() {
        when(mCustomTabIncognitoManagerMock.getProfile()).thenReturn(null);
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(mIncognitoTabMock, mSearchEngineLogoUtils);
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_IncognitoCCT_ReturnsNonPrimaryOTRProfile() {
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(mIncognitoTabMock, mSearchEngineLogoUtils);
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mNonPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_NullTab_ReturnsPrimaryOTRProfile() {
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(null, mSearchEngineLogoUtils);
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_RegularTab_ReturnsRegularProfile() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        Profile profile = regularLocationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
    }

    @Test
    @MediumTest
    public void getProfile_NullTab_ReturnsRegularProfile() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        Profile profile = regularLocationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
    }

    @Test
    @MediumTest
    public void testObserversNotified_titleChange() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onTitleChanged();

        regularLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver).onTitleChanged();

        regularLocationBarModel.setTab(mRegularTabMock, false);
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();

        regularLocationBarModel.removeObserver(mLocationBarDataObserver);
        regularLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();
    }

    @Test
    @MediumTest
    public void testObserversNotified_urlChange() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onUrlChanged();

        regularLocationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver).onUrlChanged();

        regularLocationBarModel.setTab(mRegularTabMock, false);
        verify(mLocationBarDataObserver, times(2)).onUrlChanged();

        regularLocationBarModel.removeObserver(mLocationBarDataObserver);
        regularLocationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver, times(2)).onUrlChanged();
    }

    @Test
    @MediumTest
    public void testObserversNotified_ntpLoaded() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onNtpStartedLoading();

        regularLocationBarModel.notifyNtpStartedLoading();
        verify(mLocationBarDataObserver).onNtpStartedLoading();
    }

    @Test
    @MediumTest
    public void testObserversNotified_setIsShowingTabSwitcher() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);

        verify(mLocationBarDataObserver, never()).onTitleChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged();
        verify(mLocationBarDataObserver, never()).onPrimaryColorChanged();
        verify(mLocationBarDataObserver, never()).onSecurityStateChanged();

        regularLocationBarModel.setIsShowingTabSwitcher(true);
        verify(mLocationBarDataObserver).onTitleChanged();
        verify(mLocationBarDataObserver).onUrlChanged();
        verify(mLocationBarDataObserver).onPrimaryColorChanged();
        verify(mLocationBarDataObserver).onSecurityStateChanged();
    }
}
