// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Build;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationTestHelper;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.NtpCustomizationPromoManager.SnackBarState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.url.JUnitTestGURLs;

import java.time.Duration;

/** Unit tests for {@link NtpCustomizationPromoManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
public class NtpCustomizationPromoManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Tab mTab;
    @Mock private SnackbarManager mSnackbarManager;
    private Context mContext;

    private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;

    @Before
    public void setUp() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpCustomizationPolicyManager policyManager = mock(NtpCustomizationPolicyManager.class);
        NtpCustomizationPolicyManager.setInstanceForTesting(policyManager);
        when(policyManager.isNtpCustomBackgroundEnabled()).thenReturn(true);

        mEdgeToEdgeStateProvider = NtpCustomizationTestHelper.setupEdgeToEdge(mWindowAndroid);
    }

    @After
    public void tearDown() {
        NtpCustomizationPromoManager.resetForTesting();
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mEdgeToEdgeStateProvider.detach();
    }

    @Test
    public void testCanTriggerCustomizationBottomSheet() {
        mFakeTimeTestRule.advanceMillis(Duration.ofDays(10).toMillis());
        assertTrue(
                NtpCustomizationUtils.isNtpThemeCustomizationEnabled(
                        mWindowAndroid, /* isTablet= */ false));

        // Case 1: Feature flag is disabled.
        assertFalse(ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.getValue());
        assertFalse(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.setForTesting(true);
        NtpCustomizationConfigManager configManager = mock(NtpCustomizationConfigManager.class);
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);
        when(configManager.getBackgroundType()).thenReturn(NtpBackgroundType.CHROME_COLOR);

        // Case 2: Background type is not DEFAULT.
        assertFalse(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        // Case 3: Background type is DEFAULT, but ntpOpenedCount is 1.
        when(configManager.getBackgroundType()).thenReturn(NtpBackgroundType.DEFAULT);
        assertFalse(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 1));

        // Case 4: Background type is DEFAULT, ntpOpenedCount is 2, and not shown before.
        assertTrue(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        // Case 5: Background type is DEFAULT, ntpOpenedCount is 2, but already shown.
        NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference(100);
        assertFalse(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        // Case 6: Force show enabled.
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(true);
        assertTrue(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(false);

        NtpCustomizationUtils
                .resetThemeTipBottomSheetShownTimestampFromSharedPreferenceForTesting();

        // Case 7: Within cool down period (last applied 6 days ago).
        long sixDaysAgo = TimeUtils.currentTimeMillis() - Duration.ofDays(6).toMillis();
        NtpCustomizationUtils.setLastApplyThemeTimestampToSharedPreference(sixDaysAgo);
        assertFalse(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        // Case 8: Outside cool down period (last applied 8 days ago).
        long eightDaysAgo = TimeUtils.currentTimeMillis() - Duration.ofDays(8).toMillis();
        NtpCustomizationUtils.setLastApplyThemeTimestampToSharedPreference(eightDaysAgo);

        assertEquals(
                eightDaysAgo,
                NtpCustomizationUtils.getLastApplyThemeTimestampFromSharedPreference());
        assertTrue(
                NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                        mWindowAndroid, /* isTablet= */ false, /* ntpOpenedCount= */ 2));

        // Reset for other tests.
        ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.setForTesting(false);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testCanShowCustomizationIph() {
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        // Case 1: All conditions met.
        assertTrue(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        // Case 2: Incognito.
        when(mTab.isIncognitoBranded()).thenReturn(true);
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        when(mTab.isIncognitoBranded()).thenReturn(false);

        // Case 3: Not NTP.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        // Case 4: Background type is not DEFAULT.
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.IMAGE_FROM_DISK);
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(NtpBackgroundType.DEFAULT);

        // Case 5: Customization not enabled (e.g. EdgeToEdge disabled).
        mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(0);
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));
    }

    @Test
    public void testCanShowCustomizationIph_withBottomSheetShow() {
        mFakeTimeTestRule.advanceMillis(Duration.ofDays(10).toMillis());
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        // Enable force show flag.
        ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.setForTesting(true);

        // Case 1: Bottom sheet not shown yet -> should not show IPH.
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        // Case 2: Bottom sheet shown, but within cool down period (e.g., 6 days ago).
        long sixDaysAgo = TimeUtils.currentTimeMillis() - Duration.ofDays(6).toMillis();
        NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference(sixDaysAgo);
        assertFalse(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        // Case 3: Bottom sheet shown, outside cool down period (e.g., 8 days ago).
        long eightDaysAgo = TimeUtils.currentTimeMillis() - Duration.ofDays(8).toMillis();
        NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference(eightDaysAgo);
        assertTrue(
                NtpCustomizationPromoManager.canShowCustomizationIph(
                        mTab, mWindowAndroid, /* isTablet= */ false));

        // Reset flag and preferences.
        ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.setForTesting(false);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_transitions() {
        int taskId = 123;

        // Initial state
        assertEquals(SnackBarState.NOT_SET, NtpCustomizationPromoManager.getStateForTesting());

        // NOT_SET -> PROMO_OPEN
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PROMO_OPEN, taskId);
        assertEquals(SnackBarState.PROMO_OPEN, NtpCustomizationPromoManager.getStateForTesting());

        // PROMO_OPEN -> PENDING_ON_RECREATE
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PENDING_ON_RECREATE, taskId);
        assertEquals(
                SnackBarState.PENDING_ON_RECREATE,
                NtpCustomizationPromoManager.getStateForTesting());
        assertEquals(taskId, NtpCustomizationPromoManager.getTaskIdForRecreateForTesting());
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_shownNoForceShow() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.SHOWN);

        // Try to transition to PROMO_OPEN when SHOWN and forceShow is false
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(false);
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PROMO_OPEN, taskId);
        assertEquals(SnackBarState.SHOWN, NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_shownWithForceShow() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.SHOWN);

        // Try to transition to PROMO_OPEN when SHOWN and forceShow is true
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(true);
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PROMO_OPEN, taskId);
        assertEquals(SnackBarState.PROMO_OPEN, NtpCustomizationPromoManager.getStateForTesting());

        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(false);
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_promoOpenFailsIfNotNotSet() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.PENDING_ON_RECREATE);

        // Try to transition to PROMO_OPEN when state is PENDING_ON_RECREATE and forceShow is false
        ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.setForTesting(false);
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PROMO_OPEN, taskId);
        assertEquals(
                SnackBarState.PENDING_ON_RECREATE,
                NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_pendingOnRecreateFailsIfNotPromoOpen() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.NOT_SET);

        // Try to transition to PENDING_ON_RECREATE when state is NOT_SET
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PENDING_ON_RECREATE, taskId);
        assertEquals(SnackBarState.NOT_SET, NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeShowHomepageCustomizationSnackbarOnRecreate_success() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.PENDING_ON_RECREATE);
        NtpCustomizationPromoManager.setTaskIdForRecreateForTesting(taskId);

        clearInvocations(mSnackbarManager);
        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnRecreate(
                mContext, mSnackbarManager, taskId);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
        assertEquals(SnackBarState.SHOWN, NtpCustomizationPromoManager.getStateForTesting());
        assertTrue(NtpCustomizationUtils.isThemeSnackbarShownFromSharedPreference());
    }

    @Test
    public void testMaybeShowHomepageCustomizationSnackbarOnRecreate_wrongTaskId() {
        int taskId = 123;
        int wrongTaskId = 456;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.PENDING_ON_RECREATE);
        NtpCustomizationPromoManager.setTaskIdForRecreateForTesting(taskId);

        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnRecreate(
                mContext, mSnackbarManager, wrongTaskId);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
        assertEquals(
                SnackBarState.PENDING_ON_RECREATE,
                NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeShowHomepageCustomizationSnackbarOnRecreate_wrongState() {
        int taskId = 123;
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.PROMO_OPEN);

        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnRecreate(
                mContext, mSnackbarManager, taskId);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
        assertEquals(SnackBarState.PROMO_OPEN, NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeShowHomepageCustomizationSnackbarOnDismiss_success() {
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.PROMO_OPEN);

        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnDismiss(
                mContext, mSnackbarManager);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
        assertEquals(SnackBarState.SHOWN, NtpCustomizationPromoManager.getStateForTesting());
        assertTrue(NtpCustomizationUtils.isThemeSnackbarShownFromSharedPreference());
    }

    @Test
    public void testMaybeShowHomepageCustomizationSnackbarOnDismiss_wrongState() {
        NtpCustomizationPromoManager.setStateForTesting(SnackBarState.NOT_SET);

        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnDismiss(
                mContext, mSnackbarManager);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
        assertEquals(SnackBarState.NOT_SET, NtpCustomizationPromoManager.getStateForTesting());
    }

    @Test
    public void testMaybeUpdateShowThemeTipSnackbarState_alreadyShownInSharedPreference() {
        int taskId = 123;

        // Set preference to true (already shown)
        NtpCustomizationUtils.setThemeSnackbarShownToSharedPreference(true);

        // Try to transition to PROMO_OPEN
        NtpCustomizationPromoManager.maybeUpdateShowThemeTipSnackbarState(
                SnackBarState.PROMO_OPEN, taskId);

        // State should remain NOT_SET because it was already shown
        assertEquals(SnackBarState.NOT_SET, NtpCustomizationPromoManager.getStateForTesting());
    }
}
