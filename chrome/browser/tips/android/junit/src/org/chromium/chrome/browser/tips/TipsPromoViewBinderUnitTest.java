// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.tips.TipsPromoProperties.ScreenType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

import java.util.Collections;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link TipsPromoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TipsPromoViewBinderUnitTest {
    private static final String POSITIVE_BUTTON_TEXT = "button_text";
    private static final String PROMO_TITLE = "title";
    private static final String PROMO_DESCRIPTION = "description";
    private static final String DETAILS_TITLE = "details_title";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private PropertyModel mModel;
    private TipsPromoView mView;
    private ButtonCompat mMainPagePositiveButtonView;
    private ButtonCompat mDetailPagePositiveButtonView;
    private TextView mTitleView;
    private TextView mDescriptionView;
    private TextView mDetailsTitleView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mView =
                (TipsPromoView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tips_promo_bottom_sheet, null, false);
        mMainPagePositiveButtonView = mView.getMainPagePositiveButtonForTesting();
        mDetailPagePositiveButtonView = mView.getDetailPagePositiveButtonForTesting();
        mTitleView = mView.getMainPageTitleForTesting();
        mDescriptionView = mView.getMainPageDescriptionForTesting();
        mDetailsTitleView = mView.getDetailPageTitleForTesting();

        mModel = TipsPromoProperties.createDefaultModel();
        PropertyModelChangeProcessor.create(mModel, mView, TipsPromoViewBinder::bind);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
    }

    @Test
    public void testInflationAndChildViewCaching() {
        assertNotNull(mView.getMainPagePositiveButtonForTesting());
        assertNotNull(mView.getDetailPagePositiveButtonForTesting());
        assertNotNull(mView.getMainPageTitleForTesting());
        assertNotNull(mView.getMainPageDescriptionForTesting());
        assertNotNull(mView.getDetailPageTitleForTesting());
        assertNotNull(mView.getDetailsButtonForTesting());
        assertNotNull(mView.getBackButtonForTesting());
    }

    @Test
    public void testCurrentScreen() {
        mModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.DETAIL_SCREEN);
        assertEquals(ScreenType.DETAIL_SCREEN, mView.getDisplayedChild());
    }

    @Test
    public void testFeatureTipPromoData() {
        FeatureTipPromoData promoData =
                new FeatureTipPromoData(
                        POSITIVE_BUTTON_TEXT,
                        PROMO_TITLE,
                        PROMO_DESCRIPTION,
                        R.drawable.tips_promo_esb_logo,
                        DETAILS_TITLE,
                        Collections.emptyList());
        mModel.set(TipsPromoProperties.FEATURE_TIP_PROMO_DATA, promoData);
        assertEquals(POSITIVE_BUTTON_TEXT, mMainPagePositiveButtonView.getText());
        assertEquals(POSITIVE_BUTTON_TEXT, mDetailPagePositiveButtonView.getText());
        assertEquals(PROMO_TITLE, mTitleView.getText());
        assertEquals(PROMO_DESCRIPTION, mDescriptionView.getText());
        assertEquals(DETAILS_TITLE, mDetailsTitleView.getText());
    }

    @Test
    public void testDetailsButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener = (view) -> callbackHelper.notifyCalled();

        mModel.set(TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER, clickListener);
        View onClickListener = mView.getDetailsButtonForTesting();
        assertNotNull(onClickListener);
        onClickListener.performClick();
        callbackHelper.waitForOnly();
    }

    @Test
    public void testSettingsButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener = (view) -> callbackHelper.notifyCalled();

        mModel.set(TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER, clickListener);
        View settingsOnClickListener = mView.getMainPagePositiveButtonForTesting();
        assertNotNull(settingsOnClickListener);
        settingsOnClickListener.performClick();
        callbackHelper.waitForNext();

        View settingsDetailsOnClickListener = mView.getDetailPagePositiveButtonForTesting();
        assertNotNull(settingsDetailsOnClickListener);
        settingsDetailsOnClickListener.performClick();
        callbackHelper.waitForNext();
    }

    @Test
    public void testBackButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener = (view) -> callbackHelper.notifyCalled();

        mModel.set(TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER, clickListener);
        View onClickListener = mView.getBackButtonForTesting();
        assertNotNull(onClickListener);
        onClickListener.performClick();
        callbackHelper.waitForOnly();
    }
}
