// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

import java.util.List;

/** Unit tests for {@link EnhancedSafeBrowsingTipsPromoHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EnhancedSafeBrowsingTipsPromoHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;

    private Activity mActivity;
    private EnhancedSafeBrowsingTipsPromoHandler mHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mHandler = new EnhancedSafeBrowsingTipsPromoHandler();
    }

    @Test
    public void testGetPromoData() {
        FeatureTipPromoData data = mHandler.getPromoData(mActivity);

        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                data.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_esb),
                data.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_esb),
                data.mainPageDescription);
        assertEquals(R.drawable.tips_promo_esb_logo, data.mainPageLogoViewRes);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_esb),
                data.detailPageTitle);
        assertEquals(
                List.of(
                        mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_esb),
                        mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_esb),
                        mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_esb)),
                data.detailPageSteps);
    }

    @Test
    public void testOnPromoAccepted_OpensSafeBrowsingSettings() {
        mHandler.onPromoAccepted(mActivity);

        ArgumentCaptor<Bundle> argsCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mSettingsNavigation)
                .startSettings(
                        eq(mActivity),
                        eq(SafeBrowsingSettingsFragment.class),
                        argsCaptor.capture());
        assertEquals(
                SettingsAccessPoint.TIPS_NOTIFICATIONS_PROMO,
                argsCaptor.getValue().getInt(SafeBrowsingSettingsFragment.ACCESS_POINT));
    }
}
