// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TipsPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private QuickDeleteController mQuickDeleteController;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LensController mLensController;

    private Activity mActivity;
    private TipsPromoCoordinator mTipsPromoCoordinator;
    private PropertyModel mPropertyModel;
    private View mView;
    private BottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTipsPromoCoordinator =
                new TipsPromoCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mQuickDeleteController,
                        mWindowAndroid,
                        /* isIncognito= */ false);
        mPropertyModel = mTipsPromoCoordinator.getModelForTesting();
        mView = mTipsPromoCoordinator.getViewForTesting();
        mBottomSheetContent = mTipsPromoCoordinator.getBottomSheetContentForTesting();

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
    }

    @SmallTest
    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_EnhancedSafeBrowsing() {
        when(mSettingsNavigation.createSettingsIntent(eq(mActivity), any(), any()))
                .thenReturn(new Intent());

        mTipsPromoCoordinator.showBottomSheet(TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mSettingsNavigation).createSettingsIntent(eq(mActivity), any(), any());
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_QuickDelete() {
        mTipsPromoCoordinator.showBottomSheet(TipsNotificationsFeatureType.QUICK_DELETE);

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mQuickDeleteController).showDialog();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_GoogleLens() {
        mTipsPromoCoordinator.setLensControllerForTesting(mLensController);
        mTipsPromoCoordinator.showBottomSheet(TipsNotificationsFeatureType.GOOGLE_LENS);

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mLensController).startLens(eq(mWindowAndroid), any());
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_BottomOmnibox() {
        mTipsPromoCoordinator.showBottomSheet(TipsNotificationsFeatureType.BOTTOM_OMNIBOX);

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mSettingsNavigation).startSettings(eq(mActivity), any());
    }

    @Test
    public void testSheetContent_handleBackPressDetailScreen() {
        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.DETAIL_SCREEN);
        mBottomSheetContent.handleBackPress();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
    }

    @Test
    public void testSheetContent_onBackPressedMainScreen() {
        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
        mBottomSheetContent.onBackPressed();
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }
}
