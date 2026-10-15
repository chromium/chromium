// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
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
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.quick_delete.QuickDeleteControllerProvider;
import org.chromium.chrome.browser.quick_delete.R;
import org.chromium.chrome.browser.tips.TipsPromoCustomizer;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link QuickDeleteTipsPromoHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class QuickDeleteTipsPromoHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private QuickDeleteController mQuickDeleteController;
    @Mock private TipsPromoCustomizer mCustomizer;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private QuickDeleteTipsPromoHandler mHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ false);
        mHandler = new QuickDeleteTipsPromoHandler();
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testGetPromoData() {
        FeatureTipPromoData data = mHandler.getPromoData(mActivity);

        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                data.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_quick_delete),
                data.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_quick_delete),
                data.mainPageDescription);
        assertEquals(R.raw.tips_notifications_quick_delete_logo_anim, data.mainPageLogoViewRes);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_quick_delete_short),
                data.detailPageTitle);
        assertEquals(
                List.of(
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_first_step_quick_delete),
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_second_step_quick_delete),
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_third_step_quick_delete)),
                data.detailPageSteps);
    }

    @Test
    public void testOnPromoAccepted_ShowsQuickDeleteDialog() {
        Supplier<QuickDeleteController> factory = () -> mQuickDeleteController;
        QuickDeleteControllerProvider.attach(mWindowAndroid, factory);

        mHandler.onPromoAccepted(mWindowAndroid);

        verify(mQuickDeleteController).showDialog();
        QuickDeleteControllerProvider.detach(factory);
    }

    @Test
    public void testOnPromoShown_SetsLogoAnimation() {
        mHandler.onPromoShown(mCustomizer);

        verify(mCustomizer).setLogoAnimation(R.raw.tips_notifications_quick_delete_logo_anim);
    }
}
