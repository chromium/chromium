// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.notifications.tips.TipsUtils.LOGO_IMAGE_MAX_WIDTH_RATIO;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

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

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoCoordinator.TipsPromoSheetContent;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TipsPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoCoordinatorUnitTest {
    private static final int NARROW_SCREEN_WIDTH_DP = 300;
    private static final int WIDE_SCREEN_WIDTH_DP = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private QuickDeleteController mQuickDeleteController;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    @Mock private ChromeTabCreator mTabCreator;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LensController mLensController;
    @Mock private Profile mProfile;
    @Mock private LayoutManager mLayoutManager;
    @Mock private IdentityManager mIdentityManagerMock;

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
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        mTipsPromoCoordinator =
                new TipsPromoCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mQuickDeleteController,
                        mSigninCoordinator,
                        mTabCreator,
                        mWindowAndroid,
                        /* isIncognito= */ false,
                        mProfile,
                        mLayoutManager,
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
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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

    @SmallTest
    @Test
    public void testShowBottomSheet_PasswordAutofill() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.PasswordAutofill",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.PasswordAutofill",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.PASSWORD_AUTOFILL);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_Signin() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.Signin",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.Signin",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.SIGNIN);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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
        verify(mSigninCoordinator).startSigninFlow(any());

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_Signin_UserAlreadySignedIn() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.SIGNIN);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
        assertEquals(View.GONE, mView.findViewById(R.id.tips_promo_details_button).getVisibility());
        assertEquals(
                View.GONE, mView.findViewById(R.id.main_page_description_text).getVisibility());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_CreateTabGroups() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.CreateTabGroups",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.CreateTabGroups",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.CREATE_TAB_GROUPS);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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
        verify(mLayoutManager).showLayout(eq(LayoutType.TAB_SWITCHER), eq(true));

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_CustomizeMVT() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.CustomizeMVT",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.CustomizeMVT",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.CUSTOMIZE_MVT);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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

        histogramWatcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet_RecentTabs() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.FeatureTipPromo.EventType.RecentTabs",
                                TipsPromoCoordinator.FeatureTipPromoEventType.SHOWN,
                                TipsPromoCoordinator.FeatureTipPromoEventType.ACCEPTED,
                                TipsPromoCoordinator.FeatureTipPromoEventType
                                        .DETAIL_PAGE_BACK_BUTTON)
                        .expectIntRecordTimes(
                                "Notifications.Tips.FeatureTipPromo.EventType.RecentTabs",
                                TipsPromoCoordinator.FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
                                2)
                        .build();

        setUpTipsPromoCoordinator(TipsNotificationsFeatureType.RECENT_TABS);
        mTipsPromoCoordinator.showBottomSheet();
        assertNotNull(((ImageView) mView.findViewById(R.id.main_page_logo)).getDrawable());

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
        verify(mTabCreator).createNewTab(any(), eq(TabLaunchType.FROM_CHROME_UI), eq(null));

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

    @Test
    public void testConfigurationChangeScalesImageLogo() {
        // Set up a scenario where context configuration and parameter configuration differ
        // to verify that the parameter configuration is used.

        // Default context configuration should be narrow screen (phone).
        // Assert that the width is match parent for portrait phones.
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mView.findViewById(R.id.main_page_logo).getLayoutParams().width);

        // Create a configuration parameter with wide screen (tablet)
        Configuration tabletConfig = new Configuration();
        tabletConfig.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        tabletConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Simulate configuration change with tablet config parameter
        // while context still has phone config.
        mTipsPromoCoordinator.triggerConfigurationChangeForTesting(tabletConfig);

        // Should now scale (tablet behavior) - proving it uses parameter config.
        assertEquals(
                Math.round(
                        ViewUtils.dpToPx(mActivity, WIDE_SCREEN_WIDTH_DP)
                                * LOGO_IMAGE_MAX_WIDTH_RATIO),
                mView.findViewById(R.id.main_page_logo).getLayoutParams().width,
                MathUtils.EPSILON);

        // Create another configuration parameter with narrow screen (phone).
        Configuration phoneConfig = new Configuration();
        phoneConfig.screenWidthDp = NARROW_SCREEN_WIDTH_DP;
        phoneConfig.orientation = Configuration.ORIENTATION_PORTRAIT;

        // Simulate configuration change back to phone config.
        mTipsPromoCoordinator.triggerConfigurationChangeForTesting(phoneConfig);

        // Should now be match parent (phone behavior).
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mView.findViewById(R.id.main_page_logo).getLayoutParams().width);
    }

    private void setUpTipsPromoCoordinator(@TipsNotificationsFeatureType int featureType) {
        mTipsPromoCoordinator =
                new TipsPromoCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mQuickDeleteController,
                        mSigninCoordinator,
                        mTabCreator,
                        mWindowAndroid,
                        /* isIncognito= */ false,
                        mProfile,
                        mLayoutManager,
                        featureType);
        mPropertyModel = mTipsPromoCoordinator.getModelForTesting();
        mView = mTipsPromoCoordinator.getViewForTesting();
        mBottomSheetContent = mTipsPromoCoordinator.getBottomSheetContentForTesting();
    }
}
