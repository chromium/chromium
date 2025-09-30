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
                        mActivity, TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_esb),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_esb),
                promoData.mainPageDescription);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_QuickDelete() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity, TipsNotificationsFeatureType.QUICK_DELETE);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_quick_delete),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_quick_delete),
                promoData.mainPageDescription);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_GoogleLens() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity, TipsNotificationsFeatureType.GOOGLE_LENS);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_lens),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_lens),
                promoData.mainPageDescription);
    }

    @SmallTest
    @Test
    public void testGetFeatureTipPromoDataForType_BottomOmnibox() {
        FeatureTipPromoData promoData =
                TipsUtils.getFeatureTipPromoDataForType(
                        mActivity, TipsNotificationsFeatureType.BOTTOM_OMNIBOX);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox),
                promoData.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_bottom_omnibox),
                promoData.mainPageDescription);
    }
}
