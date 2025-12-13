// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

import java.util.Collections;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link TipsPromoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private View mView;
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
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.tips_promo_bottom_sheet, null, false);
        mMainPagePositiveButtonView = mView.findViewById(R.id.tips_promo_settings_button);
        mDetailPagePositiveButtonView = mView.findViewById(R.id.tips_promo_details_settings_button);
        mTitleView = mView.findViewById(R.id.main_page_title_text);
        mDescriptionView = mView.findViewById(R.id.main_page_description_text);
        mDetailsTitleView = mView.findViewById(R.id.details_page_title_text);

        mModel = TipsPromoProperties.createDefaultModel();
        PropertyModelChangeProcessor.create(mModel, mView, TipsPromoViewBinder::bind);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
    }

    @SmallTest
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

    @SmallTest
    @Test
    public void testDetailsButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener =
                (view) -> {
                    callbackHelper.notifyCalled();
                };

        mModel.set(TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER, clickListener);
        View onClickListener = mView.findViewById(R.id.tips_promo_details_button);
        assertNotNull(onClickListener);
        onClickListener.performClick();
        callbackHelper.waitForOnly();
    }

    @SmallTest
    @Test
    public void testSettingsButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener = (view) -> callbackHelper.notifyCalled();

        mModel.set(TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER, clickListener);
        View settingsOnClickListener = mView.findViewById(R.id.tips_promo_settings_button);
        assertNotNull(settingsOnClickListener);
        settingsOnClickListener.performClick();
        callbackHelper.waitForNext();

        View settingsDetailsOnClickListener =
                mView.findViewById(R.id.tips_promo_details_settings_button);
        assertNotNull(settingsDetailsOnClickListener);
        settingsDetailsOnClickListener.performClick();
        callbackHelper.waitForNext();
    }

    @SmallTest
    @Test
    public void testBackButtonClickListener() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        OnClickListener clickListener =
                (view) -> {
                    callbackHelper.notifyCalled();
                };

        mModel.set(TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER, clickListener);
        View onClickListener = mView.findViewById(R.id.details_page_back_button);
        assertNotNull(onClickListener);
        onClickListener.performClick();
        callbackHelper.waitForOnly();
    }
}
