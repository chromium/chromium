// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link TipsPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private TipsPromoCoordinator mTipsPromoCoordinator;
    private PropertyModel mPropertyModel;
    private View mView;
    private BottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTipsPromoCoordinator = new TipsPromoCoordinator(mActivity, mBottomSheetController);
        mPropertyModel = mTipsPromoCoordinator.getModelForTesting();
        mView = mTipsPromoCoordinator.getViewForTesting();
        mBottomSheetContent = mTipsPromoCoordinator.getBottomSheetContentForTesting();
    }

    @SmallTest
    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        mTipsPromoCoordinator.showBottomSheet(
                TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING, callbackHelper::notifyCalled);

        assertEquals(
                ScreenType.MAIN_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        mView.findViewById(R.id.tips_promo_details_button).performClick();
        assertEquals(
                ScreenType.DETAIL_SCREEN, mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));

        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        mView.findViewById(R.id.tips_promo_settings_button).performClick();
        verify(mBottomSheetController).hideContent(any(), eq(true));
        callbackHelper.waitForOnly();
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
