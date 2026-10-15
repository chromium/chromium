// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
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
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tips.TipsPromoCustomizer;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Unit tests for {@link GoogleLensTipsPromoHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleLensTipsPromoHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LensController mLensController;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TipsPromoCustomizer mCustomizer;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private UserActionTester mActionTester;
    private GoogleLensTipsPromoHandler mHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ false);
        LensController.setInstanceForTesting(mLensController);
        mActionTester = new UserActionTester();
        mHandler = new GoogleLensTipsPromoHandler();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
        mWindowAndroid.destroy();
    }

    @Test
    public void testGetPromoData() {
        FeatureTipPromoData data = mHandler.getPromoData(mActivity);

        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text_lens),
                data.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_lens),
                data.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_lens),
                data.mainPageDescription);
        assertEquals(R.drawable.tips_promo_lens_logo, data.mainPageLogoViewRes);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_lens),
                data.detailPageTitle);
        assertEquals(
                List.of(
                        mActivity.getString(R.string.tips_promo_bottom_sheet_first_step_lens),
                        mActivity.getString(R.string.tips_promo_bottom_sheet_second_step_lens),
                        mActivity.getString(R.string.tips_promo_bottom_sheet_third_step_lens)),
                data.detailPageSteps);
    }

    @Test
    public void testOnPromoAccepted_StartsLens() {
        attachTabModelSelector(/* isIncognito= */ false);

        LensIntentParams params = acceptPromoAndCaptureLensParams();

        assertEquals(LensEntryPoint.TIPS_NOTIFICATIONS, params.getLensEntryPoint());
        assertFalse(params.getIsIncognito());
        assertEquals(1, mActionTester.getActionCount("Notifications.Tips.Lens"));
    }

    @Test
    public void testOnPromoAccepted_Incognito() {
        attachTabModelSelector(/* isIncognito= */ true);

        assertTrue(acceptPromoAndCaptureLensParams().getIsIncognito());
    }

    @Test
    public void testOnPromoAccepted_NoTabModelSelector_DefaultsToNonIncognito() {
        assertFalse(acceptPromoAndCaptureLensParams().getIsIncognito());
    }

    @Test
    public void testOnPromoShown_RecordsShown() {
        mHandler.onPromoShown(mCustomizer);

        assertEquals(1, mActionTester.getActionCount("Notifications.Tips.LensShown"));
    }

    private void attachTabModelSelector(boolean isIncognito) {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(isIncognito);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
    }

    private LensIntentParams acceptPromoAndCaptureLensParams() {
        mHandler.onPromoAccepted(mWindowAndroid);
        ArgumentCaptor<LensIntentParams> paramsCaptor =
                ArgumentCaptor.forClass(LensIntentParams.class);
        verify(mLensController).startLens(eq(mWindowAndroid), paramsCaptor.capture());
        return paramsCaptor.getValue();
    }
}
