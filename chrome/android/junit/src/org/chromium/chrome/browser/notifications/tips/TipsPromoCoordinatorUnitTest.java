// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoCoordinator.TipsPromoSheetContent;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
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

    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private Activity mActivity;
    private TipsPromoCoordinator mTipsPromoCoordinator;
    private PropertyModel mPropertyModel;
    private View mView;
    private TipsPromoSheetContent mBottomSheetContent;
    private UserActionTester mActionTester;

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
                        /* isIncognito= */ false,
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mPropertyModel = mTipsPromoCoordinator.getModelForTesting();
        mView = mTipsPromoCoordinator.getViewForTesting();
        mBottomSheetContent = mTipsPromoCoordinator.getBottomSheetContentForTesting();
        mActionTester = new UserActionTester();

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
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.EnhancedSafeBrowsing",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.EnhancedSafeBrowsing",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        when(mSettingsNavigation.createSettingsIntent(eq(mActivity), any(), any()))
                .thenReturn(new Intent());

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);
        mTipsPromoCoordinator.showBottomSheet();

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.details_page_back_button).performClick();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
        mView.findViewById(R.id.tips_promo_details_button).performClick();

        assertEquals(
                3,
                mPropertyModel
                        .get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA)
                        .detailPageSteps
                        .size());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mSettingsNavigation).createSettingsIntent(eq(mActivity), any(), any());

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_QuickDelete() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.QuickDelete",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.QuickDelete",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.QUICK_DELETE);
        mTipsPromoCoordinator.showBottomSheet();

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.details_page_back_button).performClick();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
        mView.findViewById(R.id.tips_promo_details_button).performClick();

        assertEquals(
                3,
                mPropertyModel
                        .get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA)
                        .detailPageSteps
                        .size());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mQuickDeleteController).showDialog();

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_GoogleLens() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.GoogleLens",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.GoogleLens",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.GOOGLE_LENS);
        mTipsPromoCoordinator.setLensControllerForTesting(mLensController);
        mTipsPromoCoordinator.showBottomSheet();
        assertEquals(1, mActionTester.getActionCount("Notifications.Tips.LensShown"));

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.details_page_back_button).performClick();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
        mView.findViewById(R.id.tips_promo_details_button).performClick();

        assertEquals(
                3,
                mPropertyModel
                        .get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA)
                        .detailPageSteps
                        .size());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mLensController).startLens(eq(mWindowAndroid), any());
        assertEquals(1, mActionTester.getActionCount("Notifications.Tips.Lens"));

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_BottomOmnibox() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.BottomOmnibox",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.BottomOmnibox",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.BOTTOM_OMNIBOX);
        mTipsPromoCoordinator.showBottomSheet();

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.details_page_back_button).performClick();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
        mView.findViewById(R.id.tips_promo_details_button).performClick();

        assertEquals(
                3,
                mPropertyModel
                        .get(TipsPromoProperties.FEATURE_TIP_PROMO_DATA)
                        .detailPageSteps
                        .size());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        verify(mSettingsNavigation).startSettings(eq(mActivity), any(), any());

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSheetContent_handleBackPressDetailScreen() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.FeatureTipPromo.EventType.EnhancedSafeBrowsing",
                        TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_BACK_BUTTON);

        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.DETAIL_SCREEN);
        mBottomSheetContent.handleBackPress();
        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSheetContent_onBackPressedMainScreen() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.FeatureTipPromo.EventType.EnhancedSafeBrowsing",
                        TipsPromoCoordinator.FeatureTipPromoEventType.DISMISSED);

        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
        mBottomSheetContent.onBackPressed();
        verify(mBottomSheetController).hideContent(any(), eq(true));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testBottomSheetObserver() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.EnhancedSafeBrowsing",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DISMISSED,
                                2)
                        .build();

        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetOpened(StateChangeReason.NONE);

        observer.onSheetClosed(StateChangeReason.TAP_SCRIM);
        verify(mBottomSheetController).removeObserver(eq(observer));

        clearInvocations(mBottomSheetController);

        observer.onSheetClosed(StateChangeReason.SWIPE);
        verify(mBottomSheetController).removeObserver(eq(observer));

        histogramWatcher.assertExpected();
    }

    private void setUpTipsPromoCoordinator(@TipsNotificationsFeatureType int featureType) {
        mTipsPromoCoordinator =
                new TipsPromoCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mQuickDeleteController,
                        mWindowAndroid,
                        /* isIncognito= */ false,
                        featureType);
        mPropertyModel = mTipsPromoCoordinator.getModelForTesting();
        mView = mTipsPromoCoordinator.getViewForTesting();
        mBottomSheetContent = mTipsPromoCoordinator.getBottomSheetContentForTesting();
    }
}
