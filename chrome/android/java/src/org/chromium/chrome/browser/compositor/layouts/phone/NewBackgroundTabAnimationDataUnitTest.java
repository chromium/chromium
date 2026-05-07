// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.toolbar.settings.AddressBarPreference;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link NewBackgroundTabAnimationData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewBackgroundTabAnimationDataUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final NonNullObservableSupplier<Float> mNtpSearchBoxTransitionPercentageSupplier =
            ObservableSuppliers.createNonNull(0f);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ToolbarManager mToolbarManager;
    @Mock private Tab mTab;
    @Mock private View mToolbarTabSwitcherButton;
    @Mock private View mBottomBarTabSwitcherButton;
    @Mock private View mRootView;
    @Mock private View mToolbarContainer;
    @Mock private View mBottomBarContainer;
    @Mock private NtpCustomizationConfigManager mMockConfigManager;
    @Mock private NtpCustomizationPolicyManager mMockPolicyManager;

    private Activity mActivity;
    private NewBackgroundTabAnimationData mData;
    private int mToolbarButtonWidth;
    private int mToolbarHeight;
    private int mBottomBarHeight;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        when(mToolbarManager.getNtpSearchBoxTransitionPercentageSupplier())
                .thenReturn(mNtpSearchBoxTransitionPercentageSupplier);

        NtpCustomizationConfigManager.setInstanceForTesting(mMockConfigManager);
        NtpCustomizationPolicyManager.setInstanceForTesting(mMockPolicyManager);
    }

    @After
    public void tearDown() {
        NtpCustomizationConfigManager.setInstanceForTesting(null);
        NtpCustomizationPolicyManager.setInstanceForTesting(null);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        when(mRootView.getContext()).thenReturn(mActivity);
        when(mRootView.findViewById(R.id.toolbar)).thenReturn(mToolbarContainer);
        when(mRootView.findViewById(
                        org.chromium.chrome.browser.ui.bottombar.R.id.bottom_bar_container))
                .thenReturn(mBottomBarContainer);

        when(mToolbarContainer.findViewById(R.id.tab_switcher_button))
                .thenReturn(mToolbarTabSwitcherButton);
        when(mBottomBarContainer.findViewById(R.id.tab_switcher_button))
                .thenReturn(mBottomBarTabSwitcherButton);

        Resources res = activity.getResources();
        mToolbarButtonWidth = res.getDimensionPixelSize(R.dimen.toolbar_button_width);
        mToolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        mBottomBarHeight = res.getDimensionPixelSize(R.dimen.bottom_bar_height);

        when(mToolbarTabSwitcherButton.getGlobalVisibleRect(any(Rect.class)))
                .thenAnswer(
                        invocation -> {
                            Rect r = invocation.getArgument(0);
                            if (ToolbarPositionController.shouldShowToolbarOnTop(mTab)) {
                                r.set(0, 0, mToolbarButtonWidth, mToolbarHeight);
                            } else {
                                // Simulate toolbar being at the bottom of the screen (height 2000).
                                r.set(0, 2000 - mToolbarHeight, mToolbarButtonWidth, 2000);
                            }
                            return true;
                        });

        when(mBottomBarTabSwitcherButton.getGlobalVisibleRect(any(Rect.class)))
                .thenAnswer(
                        invocation -> {
                            Rect r = invocation.getArgument(0);
                            r.set(0, 2000 - mBottomBarHeight, mToolbarButtonWidth, 2000);
                            return true;
                        });
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testCaptureState_BottomBarDisabled() {
        when(mToolbarManager.getPrimaryColor()).thenReturn(Color.RED);
        mData = new NewBackgroundTabAnimationData(mRootView, mToolbarManager);
        mData.captureState(mTab, /* isRegularNtp= */ false, /* expectedToolbarTop= */ 100);

        assertEquals(mToolbarTabSwitcherButton, mData.getTabSwitcherButton());
        assertFalse(mData.isBottomBarVisible());
        assertTrue(mData.isPositionOnTop());

        Rect tabSwitcherButtonRect = mData.getTabSwitcherButtonRect();
        assertEquals(mToolbarButtonWidth, tabSwitcherButtonRect.width());
        assertEquals(mToolbarHeight, tabSwitcherButtonRect.height());

        assertEquals(100, tabSwitcherButtonRect.top);
        assertEquals(100 + mToolbarHeight, tabSwitcherButtonRect.bottom);

        assertEquals(Color.RED, mData.getPrimaryColor());
        assertEquals(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT, mData.getAnimationType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false")
    public void testCaptureState_BottomBarEnabled_Visible() {
        mData = new NewBackgroundTabAnimationData(mRootView, mToolbarManager);
        mData.captureState(mTab, /* isRegularNtp= */ false, /* expectedToolbarTop= */ 0);

        assertEquals(mBottomBarTabSwitcherButton, mData.getTabSwitcherButton());
        assertTrue(mData.isBottomBarVisible());
        assertFalse(mData.isPositionOnTop());

        Rect tabSwitcherButtonRect = mData.getTabSwitcherButtonRect();
        assertEquals(mToolbarButtonWidth, tabSwitcherButtonRect.width());
        assertEquals(mBottomBarHeight, tabSwitcherButtonRect.height());

        assertEquals(2000 - mBottomBarHeight, tabSwitcherButtonRect.top);
        assertEquals(2000, tabSwitcherButtonRect.bottom);

        assertEquals(
                NewBackgroundTabAnimationHostView.AnimationType.DEFAULT, mData.getAnimationType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true")
    public void testCaptureState_BottomBarEnabled_NtpDisabled() {
        mData = new NewBackgroundTabAnimationData(mRootView, mToolbarManager);
        mData.captureState(mTab, /* isRegularNtp= */ true, /* expectedToolbarTop= */ 0);

        assertEquals(mToolbarTabSwitcherButton, mData.getTabSwitcherButton());
        assertFalse(mData.isBottomBarVisible());

        Rect tabSwitcherButtonRect = mData.getTabSwitcherButtonRect();
        assertEquals(mToolbarButtonWidth, tabSwitcherButtonRect.width());
        assertEquals(mToolbarHeight, tabSwitcherButtonRect.height());
        assertEquals(0, tabSwitcherButtonRect.top);
        assertEquals(mToolbarHeight, tabSwitcherButtonRect.bottom);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testCaptureState_NtpAdjustment() {
        when(mMockPolicyManager.isNtpCustomBackgroundEnabled()).thenReturn(true);
        when(mMockConfigManager.getBackgroundType()).thenReturn(NtpBackgroundType.IMAGE_FROM_DISK);

        mData = new NewBackgroundTabAnimationData(mRootView, mToolbarManager);
        mData.captureState(mTab, /* isRegularNtp= */ true, /* expectedToolbarTop= */ 0);

        assertEquals(BrandedColorScheme.DARK_BRANDED_THEME, mData.getBrandedColorScheme());
        assertEquals(mToolbarTabSwitcherButton, mData.getTabSwitcherButton());

        Rect tabSwitcherButtonRect = mData.getTabSwitcherButtonRect();
        assertEquals(mToolbarButtonWidth, tabSwitcherButtonRect.width());
        assertEquals(mToolbarHeight, tabSwitcherButtonRect.height());
        assertEquals(0, tabSwitcherButtonRect.top);
        assertEquals(mToolbarHeight, tabSwitcherButtonRect.bottom);
    }

    @Test
    // Disable extra logs to avoid Native / LocalStatePrefs read in
    // AddressBarPreference#setToolbarPositionAndSource.
    @DisableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS
    })
    public void testCaptureState_ToolbarPositionBottom() {
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.BOTTOM_SETTINGS);

        mData = new NewBackgroundTabAnimationData(mRootView, mToolbarManager);
        mData.captureState(mTab, /* isRegularNtp= */ false, /* expectedToolbarTop= */ 100);

        assertEquals(mToolbarTabSwitcherButton, mData.getTabSwitcherButton());
        assertFalse(mData.isBottomBarVisible());
        assertFalse(mData.isPositionOnTop());

        Rect tabSwitcherButtonRect = mData.getTabSwitcherButtonRect();
        assertEquals(2000 - mToolbarHeight, tabSwitcherButtonRect.top);
        assertEquals(2000, tabSwitcherButtonRect.bottom);

        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.TOP_SETTINGS);
    }
}
