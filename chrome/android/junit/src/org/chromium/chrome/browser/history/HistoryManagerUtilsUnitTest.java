// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.DeviceInfo;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link HistoryManagerUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_WEB_UI_HISTORY)
public class HistoryManagerUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private UrlConstantResolver mUrlConstantResolver;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private IncognitoHistoryDisclaimerCoordinator mDisclaimerCoordinator;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfile(any())).thenReturn("profile_token");
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mIncognitoTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentTabModelSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(mIncognitoTabModel));
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mActivityController = Robolectric.buildActivity(TestActivity.class);
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivityController.setup();
        mActivity.setContentView(new View(mActivity));
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mTab.isIncognitoBranded()).thenReturn(true);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mUrlConstantResolver.getNtpUrl()).thenReturn(UrlConstants.NTP_URL);
        UrlConstantResolverFactory.setForTesting(mUrlConstantResolver);
        mDisclaimerCoordinator =
                new IncognitoHistoryDisclaimerCoordinator(
                        mActivity, mWindowAndroid, new int[] {View.generateViewId()});
    }

    @After
    public void tearDown() {
        mDisclaimerCoordinator.destroy();
        mActivityController.destroy();
    }

    @Test
    public void testShowHistoryManager_offTheRecord_showsDisclaimerPopup() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        HistoryManagerUtils.showHistoryManager(mWindowAndroid, /* tab= */ null, mProfile);

        assertTrue(mDisclaimerCoordinator.isShowing());
        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());
        assertNull(Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity());
    }

    @Test
    public void testShowHistoryManager_nullWindowAndroid_doesNothing() {
        HistoryManagerUtils.showHistoryManager(
                /* windowAndroid= */ null, /* tab= */ null, mProfile);

        assertFalse(mDisclaimerCoordinator.isShowing());
        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());
        assertNull(Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_DESKTOP_WEB_UI_HISTORY)
    public void testShowHistoryManager_offTheRecordFlagOff_opensHistoryActivity() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        HistoryManagerUtils.showHistoryManager(mWindowAndroid, /* tab= */ null, mProfile);

        assertFalse(mDisclaimerCoordinator.isShowing());
        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(HistoryActivity.class.getName(), intent.getComponent().getClassName());
    }

    @Test
    public void testShowHistoryManager_regularProfile_opensHistoryTab() {
        when(mProfile.isOffTheRecord()).thenReturn(false);

        HistoryManagerUtils.showHistoryManager(mWindowAndroid, /* tab= */ null, mProfile);

        assertFalse(mDisclaimerCoordinator.isShowing());
        Intent intent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(UrlConstants.HISTORY_URL, intent.getDataString());
    }

    @Test
    public void testMaybeInterceptLoadUrl_historyUrls_interceptAndShowPopup() {
        String[] urls = {
            "chrome://history",
            "chrome://history/",
            "chrome://history/syncedTabs",
            "chrome://history/?q=foo",
            "chrome-native://history/",
            "CHROME://HISTORY/",
        };
        for (String url : urls) {
            assertTrue(url, HistoryManagerUtils.maybeInterceptLoadUrl(mTab, url));
        }

        assertTrue(mDisclaimerCoordinator.isShowing());
        verify(mTab, never()).loadUrl(any());
    }

    @Test
    public void testMaybeInterceptLoadUrl_nonHistoryUrls_returnsFalse() {
        String[] urls = {
            "https://history.example.com/",
            "https://example.com/history",
            "chrome://settings/",
            "chrome://historyx/",
            "about:history",
            "",
        };
        for (String url : urls) {
            assertFalse(url, HistoryManagerUtils.maybeInterceptLoadUrl(mTab, url));
        }

        assertFalse(mDisclaimerCoordinator.isShowing());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_DESKTOP_WEB_UI_HISTORY)
    public void testMaybeInterceptLoadUrl_flagOff_returnsFalse() {
        assertFalse(HistoryManagerUtils.maybeInterceptLoadUrl(mTab, "chrome://history/"));
        assertFalse(mDisclaimerCoordinator.isShowing());
    }

    @Test
    public void testMaybeInterceptLoadUrl_notIncognitoOrNotDesktop_returnsFalse() {
        when(mTab.isIncognitoBranded()).thenReturn(false);
        assertFalse(HistoryManagerUtils.maybeInterceptLoadUrl(mTab, "chrome://history/"));

        when(mTab.isIncognitoBranded()).thenReturn(true);
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(HistoryManagerUtils.maybeInterceptLoadUrl(mTab, "chrome://history/"));

        assertFalse(mDisclaimerCoordinator.isShowing());
    }

    @Test
    public void testMaybeInterceptLoadUrl_initialTabWithoutCommittedEntry_loadsIncognitoNtp() {
        when(mNavigationController.isInitialNavigation()).thenReturn(true);

        assertTrue(HistoryManagerUtils.maybeInterceptLoadUrl(mTab, "chrome://history/"));

        verify(mTab)
                .loadUrl(
                        argThat(
                                (LoadUrlParams params) ->
                                        UrlConstants.NTP_URL.equals(params.getUrl())
                                                && params.getTransitionType()
                                                        == PageTransition.AUTO_TOPLEVEL));
        assertTrue(mDisclaimerCoordinator.isShowing());
    }

    @Test
    public void testMaybeInterceptLoadUrl_nullWebContents_doesNotCrashOrLoadNtp() {
        when(mTab.getWebContents()).thenReturn(null);

        assertTrue(HistoryManagerUtils.maybeInterceptLoadUrl(mTab, "chrome://history/"));

        verify(mTab, never()).loadUrl(any());
        assertTrue(mDisclaimerCoordinator.isShowing());
    }
}
