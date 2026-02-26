// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;

/** Unit tests for {@link TipsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsUtilsUnitTest {
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_EnhancedSafeBrowsing() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_esb),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_esb),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_esb),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_esb),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_esb),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_esb),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_QuickDelete() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.QUICK_DELETE,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_quick_delete),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_quick_delete),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_quick_delete),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_quick_delete),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_quick_delete),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_quick_delete_short),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_GoogleLens() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.GOOGLE_LENS,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_lens),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_lens),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_lens),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_lens),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_lens),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_lens),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_lens),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_BottomOmnibox() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.BOTTOM_OMNIBOX,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_bottom_omnibox),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_PasswordAutofill() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.PASSWORD_AUTOFILL,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_noop),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_password_autofill),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_password_autofill),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_password_autofill),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_password_autofill),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_password_autofill),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_password_autofill),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_Signin() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.SIGNIN,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.signin_promo_signin), promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.educational_tip_sign_in_promo_title),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.educational_tip_sign_in_promo_description),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_signin),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_signin),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_signin),
                promoData.detailPageSteps.get(2));
        assertEquals(mActivity.getString(R.string.sign_in_to_chrome), promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_Signin_UserAlreadySignedIn() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity, TipsNotificationsFeatureType.SIGNIN, /* isUserSignedIn= */ true);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_noop),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_signin_signed_in),
                promoData.mainPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_CreateTabGroups() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.CREATE_TAB_GROUPS,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_try_now),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.educational_tip_tab_group_title),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.educational_tip_tab_group_description),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_create_tab_groups),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_create_tab_groups),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_create_tab_groups),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.educational_tip_tab_group_title),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_CustomizeMVT() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.CUSTOMIZE_MVT,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_noop),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_customize_mvt),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_customize_mvt),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_customize_mvt),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_customize_mvt),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_customize_mvt),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_customize_mvt),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_RecentTabs() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity,
                        TipsNotificationsFeatureType.RECENT_TABS,
                        /* isUserSignedIn= */ false);
        assertEquals(
                mActivity.getString(
                        R.string.tips_promo_bottom_sheet_positive_button_text_recent_tabs),
                promoData.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_recent_tabs),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_recent_tabs),
                promoData.mainPageDescription);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_recent_tabs),
                promoData.detailPageSteps.get(0));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_recent_tabs),
                promoData.detailPageSteps.get(1));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_recent_tabs),
                promoData.detailPageSteps.get(2));
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_recent_tabs),
                promoData.detailPageTitle);
    }

    @SmallTest
    @Test
    public void testGetDetailStepBackground_SingleStep() {
        assertEquals(
                R.drawable.view_list_single_item_background,
                TipsUtils.getDetailStepBackground(/* stepIndex= */ 0, /* stepCount= */ 1));
    }

    @SmallTest
    @Test
    public void testGetDetailStepBackground_FirstStep() {
        assertEquals(
                R.drawable.view_list_top_item_background,
                TipsUtils.getDetailStepBackground(/* stepIndex= */ 0, /* stepCount= */ 3));
    }

    @SmallTest
    @Test
    public void testGetDetailStepBackground_LastStep() {
        assertEquals(
                R.drawable.view_list_bottom_item_background,
                TipsUtils.getDetailStepBackground(/* stepIndex= */ 2, /* stepCount= */ 3));
    }

    @SmallTest
    @Test
    public void testGetDetailStepBackground_MiddleStep() {
        assertEquals(
                R.drawable.view_list_normal_item_background,
                TipsUtils.getDetailStepBackground(/* stepIndex= */ 1, /* stepCount= */ 3));
    }
}
