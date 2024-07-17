// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.FAIL_TO_SHOW_HOME_SURFACE_UI_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_RETURN_TIME_SECONDS;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_AT_STARTUP_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_UMA;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.format.DateUtils;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.BaseSwitches;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil.FailToShowHomeSurfaceReason;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepagePolicyManager;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReturnToChromeUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowHomepagePolicyManager.class})
@CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
public class ReturnToChromeUtilUnitTest {
    @Implements(HomepagePolicyManager.class)
    static class ShadowHomepagePolicyManager {
        static boolean sIsInitialized;

        @Implementation
        public static boolean isInitializedWithNative() {
            return sIsInitialized;
        }
    }

    private static final int ON_RETURN_THRESHOLD_SECOND = 1000;
    private static final int DELTA_MS = 100;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ChromeInactivityTracker mInactivityTracker;
    @Mock private Resources mResources;
    @Mock private TabModel mCurrentTabModel;
    @Mock private TabCreator mTabCreater;
    @Mock private Tab mTab1;
    @Mock private Tab mNtpTab;
    @Mock private NewTabPage mNewTabPage;
    @Mock private HomeSurfaceTracker mHomeSurfaceTracker;
    @Mock private Bundle mSaveInstanceState;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Mock private HomepageManager mHomepageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(mNtpTab).getUrl();

        // HomepageManager:
        HomepageManager.setInstanceForTesting(mHomepageManager);
        doReturn(true).when(mHomepageManager).isHomepageEnabled();
        doReturn(UrlConstants.ntpGurl()).when(mHomepageManager).getHomepageGurl();

        ShadowHomepagePolicyManager.sIsInitialized = true;
        assertTrue(HomepagePolicyManager.isInitializedWithNative());

        // Low end devices:
        Assert.assertFalse(SysUtils.isLowEndDevice());

        // Sets accessibility:
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);

        // Sets for phones, i.e., !DeviceFormFactor.isNonMultiDisplayContextOnTablet():
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET - 1)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcher() {
        Assert.assertEquals(
                HOME_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                HOME_SURFACE_RETURN_TIME_SECONDS.getValue());

        long returnTimeMs =
                HOME_SURFACE_RETURN_TIME_SECONDS.getValue() * DateUtils.SECOND_IN_MILLIS;
        // When return time doesn't arrive, return false:
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs + DELTA_MS));

        // When return time arrives, return true:
        assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1));
    }

    @Test
    @SmallTest
    public void testShouldShowNtpAsHomeSurfaceAtStartup() {
        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        HOME_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Tests the case when there isn't any Tab. Verifies that home surface NTP is shown.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        assertTrue(HomepagePolicyManager.isInitializedWithNative());

        assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        intent, null, mInactivityTracker));

        // Tests the case when the total tab count > 0. Verifies that home surface NTP is shown
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        intent, null, mInactivityTracker));
    }

    @Test
    @SmallTest
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithExistingNtp() {
        doReturn(2).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab).when(mCurrentTabModel).getTabAt(1);

        // Sets the NTP is the last active Tab.
        doReturn(1).when(mCurrentTabModel).index();

        // Tests case of the last active NTP has home surface UI.
        doReturn(true).when(mHomeSurfaceTracker).canShowHomeSurface(mNtpTab);
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();

        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mCurrentTabModel, never()).setIndex(anyInt(), eq(TabSelectionType.FROM_USER));
        verify(mNewTabPage, never()).showMagicStack(any());
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(null));
        histogram.assertExpected();

        // Tests the case of the last active NTP doesn't has home surface UI.
        doReturn(false).when(mHomeSurfaceTracker).canShowHomeSurface(mNtpTab);
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mHomeSurfaceTracker, times(2))
                .updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(null));
        histogram.assertExpected();

        // Sets the last active Tab isn't a NTP.
        doReturn(0).when(mCurrentTabModel).index();

        // Verifies that if the NTP isn't the last active Tab, we reuse it, set index and call
        // showHomeSurfaceUi() to show the single tab card module.
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mCurrentTabModel).setIndex(eq(1), eq(TabSelectionType.FROM_USER));
        verify(mNewTabPage).showMagicStack(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithoutAnyExistingNtp() {
        doReturn(1).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        // Verifies that if the return time doesn't arrive, there isn't a new NTP is created.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ false,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(any(), any());

        // Verifies that a new NTP is created when there isn't any existing one to reuse.
        doReturn(2).when(mNtpTab).getId();
        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab)
                .when(mTabCreater)
                .createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        doReturn(0).when(mCurrentTabModel).index();

        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, times(1)).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mNewTabPage).showMagicStack(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithMixedNtps() {
        doReturn(3).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(mNtpTab).getUrl();
        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab).when(mCurrentTabModel).getTabAt(1);

        Tab activeNtpTab = Mockito.mock(Tab.class);
        NewTabPage activeNtp = Mockito.mock(NewTabPage.class);
        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(activeNtpTab).getUrl();
        doReturn(true).when(activeNtpTab).isNativePage();
        doReturn(activeNtp).when(activeNtpTab).getNativePage();
        doReturn(activeNtpTab).when(mCurrentTabModel).getTabAt(2);

        // Set the active NTP tab as the last Tab, and has a tracking Tab.
        doReturn(2).when(mCurrentTabModel).index();
        doReturn(true).when(mHomeSurfaceTracker).canShowHomeSurface(activeNtpTab);

        // Verifies that the first found NTP isn't the active NTP Tab.
        Assert.assertEquals(
                1, TabModelUtils.getTabIndexByUrl(mCurrentTabModel, UrlConstants.NTP_URL));
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        // Verifies the active NTP will be shown with its home surface UI, not the first found NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        histogram.assertExpected();
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), any());
        verify(mNewTabPage, never()).showMagicStack(any());

        // Set the last active NTP doesn't have a tracking Tab.
        doReturn(false).when(mHomeSurfaceTracker).canShowHomeSurface(activeNtpTab);
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        // Verifies the active NTP will be shown as it is now, i.e., an empty NTP, not the first
        // found NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        histogram.assertExpected();
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), any());
        verify(mNewTabPage, never()).showMagicStack(any());
    }

    @Test
    @SmallTest
    public void testNoAnyTabCase() {
        doReturn(0).when(mCurrentTabModel).getCount();

        // Verifies that if there isn't any existing Tab, we don't create a home surface NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID})
    public void testColdStartupWithOnlyLastActiveTabUrl_MagicStack() {
        assertTrue(HomeModulesMetricsUtils.useMagicStack());

        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mNtpTab.isNativePage()).thenReturn(true);
        when(mNtpTab.getNativePage()).thenReturn(mNewTabPage);

        when(mTabCreater.createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null)))
                .thenReturn(mNtpTab);
        when(mTabModelSelector.getModel(false)).thenReturn(mCurrentTabModel);

        // Tests the case that a new NTP is created and waits for its tracking last active Tab being
        // restored.
        ReturnToChromeUtil.createNewTabAndShowHomeSurfaceUi(
                mTabCreater,
                mHomeSurfaceTracker,
                mTabModelSelector,
                JUnitTestGURLs.URL_1.getSpec(),
                null);
        verify(mCurrentTabModel).addObserver(mTabModelObserverCaptor.capture());

        // Verifies if the added Tab matches the tracking URL, call showHomeSurfaceUi().
        mTabModelObserverCaptor.getValue().willAddTab(mTab1, TabLaunchType.FROM_RESTORE);
        verify(mNewTabPage).showMagicStack(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
    }

    @Test
    @SmallTest
    public void testShouldNotShowNtpOnRecreate() {
        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        HOME_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // There should always be at least 1 tab. Otherwise one will be created regardless.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        assertTrue(HomepagePolicyManager.isInitializedWithNative());

        assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        intent, mSaveInstanceState, mInactivityTracker));

        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(ChromeActivity.IS_FROM_RECREATING, false);
        assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        intent, mSaveInstanceState, mInactivityTracker));

        doReturn(false)
                .when(mSaveInstanceState)
                .getBoolean(ChromeActivity.IS_FROM_RECREATING, false);
        assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        intent, mSaveInstanceState, mInactivityTracker));
    }

    @Test
    @SmallTest
    public void testLogFailToShowHomeSurfaceUI() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE)
                        .build();
        doReturn(null).when(mNtpTab).getNativePage();
        ReturnToChromeUtil.showHomeSurfaceUiOnNtp(mNtpTab, mTab1, mHomeSurfaceTracker);
        histogram.assertExpected();

        FrozenNativePage frozenNativePage = Mockito.mock(FrozenNativePage.class);
        doReturn(true).when(frozenNativePage).isFrozen();
        doReturn(frozenNativePage).when(mNtpTab).getNativePage();
        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE)
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN)
                        .build();
        ReturnToChromeUtil.showHomeSurfaceUiOnNtp(mNtpTab, mTab1, mHomeSurfaceTracker);
        histogram.assertExpected();
    }

    private void setupAndVerifyTablets() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
    }

    private Intent createMainIntentFromLauncher() {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        return intent;
    }
}
