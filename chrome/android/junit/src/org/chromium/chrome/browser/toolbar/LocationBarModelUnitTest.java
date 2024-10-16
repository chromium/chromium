// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.toolbar.LocationBarModelUnitTest.ShadowTrustedCdn;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.omnibox.OmniboxUrlEmphasizerJni;
import org.chromium.url.GURL;

/** Unit tests for the LocationBarModel. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowTrustedCdn.class})
@DisableFeatures(ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS)
public class LocationBarModelUnitTest {
    @Implements(TrustedCdn.class)
    static class ShadowTrustedCdn {
        @Implementation
        public static GURL getPublisherUrl(@Nullable Tab tab) {
            return null;
        }
    }

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mIncognitoTabMock;
    @Mock private Tab mIncognitoNonPrimaryTabMock;
    @Mock private Tab mRegularTabMock;

    @Mock private Profile mRegularProfileMock;
    @Mock private Profile mPrimaryOTRProfileMock;
    @Mock private Profile mNonPrimaryOTRProfileMock;

    @Mock private LocationBarDataProvider.Observer mLocationBarDataObserver;
    @Mock private LocationBarModel.Natives mLocationBarModelJni;
    @Mock private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private OmniboxUrlEmphasizerJni mOmniboxUrlEmphasizerJni;
    @Mock private LayoutStateProvider mLayoutStateProvider;

    private GURL mExampleGurl = new GURL("http://www.example.com/");

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        mJniMocker.mock(LocationBarModelJni.TEST_HOOKS, mLocationBarModelJni);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
        mJniMocker.mock(OmniboxUrlEmphasizerJni.TEST_HOOKS, mOmniboxUrlEmphasizerJni);

        when(mPrimaryOTRProfileMock.isOffTheRecord()).thenReturn(true);
        when(mNonPrimaryOTRProfileMock.isOffTheRecord()).thenReturn(true);

        when(mRegularTabMock.getProfile()).thenReturn(mRegularProfileMock);

        when(mIncognitoTabMock.isIncognito()).thenReturn(true);
        when(mIncognitoTabMock.getProfile()).thenReturn(mPrimaryOTRProfileMock);

        when(mIncognitoNonPrimaryTabMock.isIncognito()).thenReturn(true);
        when(mIncognitoNonPrimaryTabMock.getProfile()).thenReturn(mNonPrimaryOTRProfileMock);
    }

    public static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(Tab tab) {
                    return false;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return false;
                }
            };

    private static class TestLocationBarModel extends LocationBarModel {
        public TestLocationBarModel() {
            super(
                    new ContextThemeWrapper(
                            ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight),
                    NewTabPageDelegate.EMPTY,
                    url -> url.getSpec(),
                    OFFLINE_STATUS);
        }
    }

    @Test
    @MediumTest
    public void getProfile_RegularTab_ReturnsRegularProfile() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        Profile profile = locationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void getProfile_IncognitoTab_ReturnsPrimaryOTRProfile() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.setTab(mIncognitoTabMock, mPrimaryOTRProfileMock);
        Profile otrProfile = locationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOTRProfileMock, otrProfile);
        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void getProfile_IncognitoTab_ReturnsNonPrimaryOTRProfile() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.setTab(mIncognitoNonPrimaryTabMock, mNonPrimaryOTRProfileMock);
        Profile otrProfile = locationBarModel.getProfile();
        Assert.assertEquals(mNonPrimaryOTRProfileMock, otrProfile);
        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void getProfile_NullTab() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.setTab(null, mRegularProfileMock);
        Assert.assertEquals(mRegularProfileMock, locationBarModel.getProfile());

        locationBarModel.setTab(null, mPrimaryOTRProfileMock);
        Assert.assertEquals(mPrimaryOTRProfileMock, locationBarModel.getProfile());

        locationBarModel.setTab(null, mNonPrimaryOTRProfileMock);
        Assert.assertEquals(mNonPrimaryOTRProfileMock, locationBarModel.getProfile());

        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void testObserversNotified_titleChange() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onTitleChanged();

        locationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver).onTitleChanged();

        locationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();

        locationBarModel.removeObserver(mLocationBarDataObserver);
        locationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();

        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void testObserversNotified_urlChange() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.initializeWithNative();
        locationBarModel.addObserver(mLocationBarDataObserver);

        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        locationBarModel.updateVisibleGurl();

        // The visible url should be cached and hasn't changed, so onUrlChanged shouldn't be called
        locationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged();

        // Setting to a new tab with a different url
        GURL mExampleGurl2 = new GURL("http://www.example2.com/");
        doReturn(mExampleGurl2)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        locationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        verify(mLocationBarDataObserver).onUrlChanged();

        locationBarModel.removeObserver(mLocationBarDataObserver);
        locationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver).onUrlChanged();

        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void testObserversNotified_ntpLoaded() {
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onNtpStartedLoading();

        locationBarModel.notifyNtpStartedLoading();
        verify(mLocationBarDataObserver).onNtpStartedLoading();
    }

    @Test
    @MediumTest
    public void testObserversNotified_setIsShowingTabSwitcher() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.initializeWithNative();
        locationBarModel.addObserver(mLocationBarDataObserver);
        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        locationBarModel.updateVisibleGurl();

        verify(mLocationBarDataObserver, never()).onTitleChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged();
        verify(mLocationBarDataObserver, never()).onPrimaryColorChanged();
        verify(mLocationBarDataObserver, never()).onSecurityStateChanged();

        locationBarModel.updateForNonStaticLayout();

        // The omnibox is not showing, and we have not switched to a new tab yet, so don't expect
        // notifications of a url change
        verify(mLocationBarDataObserver, never()).onUrlChanged();
        Assert.assertEquals(locationBarModel.getCurrentGurl(), mExampleGurl);

        verify(mLocationBarDataObserver).onTitleChanged();
        verify(mLocationBarDataObserver).onPrimaryColorChanged();
        verify(mLocationBarDataObserver).onSecurityStateChanged();

        locationBarModel.destroy();
    }

    @Test
    @MediumTest
    public void testSpannableCache() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        LocationBarModel locationBarModel = new TestLocationBarModel();
        locationBarModel.setTab(mRegularTabMock, mRegularProfileMock);
        doReturn(true).when(mRegularTabMock).isInitialized();
        locationBarModel.initializeWithNative();

        doReturn(mExampleGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(mExampleGurl.getSpec())
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(mExampleGurl.getSpec())
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong(), Mockito.any());
        Assert.assertTrue(locationBarModel.updateVisibleGurl());
        Assert.assertFalse("Update should be suppressed", locationBarModel.updateVisibleGurl());

        // URL changed, cache is invalid.
        GURL exampleGurl2 = new GURL("http://www.example2.com/");
        doReturn(exampleGurl2)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(exampleGurl2.getSpec())
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(exampleGurl2.getSpec())
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong(), Mockito.any());
        Assert.assertTrue("New url should notify", locationBarModel.updateVisibleGurl());
        Assert.assertFalse(
                "Update should be suppressed again", locationBarModel.updateVisibleGurl());
        locationBarModel.destroy();
    }
}
