// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TipsPromoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoViewBinderUnitTest {
    private static final String PROMO_TITLE = "title";
    private static final String PROMO_DESCRIPTION = "description";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityScenario<TestActivity> mActivityScenario;
    private Activity mActivity;
    private PropertyModel mModel;
    private View mView;
    private TextView mTitleView;
    private TextView mDescriptionView;

    @Before
    public void setUp() {
        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.tips_promo_bottom_sheet, null, false);
        mTitleView = mView.findViewById(R.id.main_page_title_text);
        mDescriptionView = mView.findViewById(R.id.main_page_description_text);

        mModel = TipsPromoProperties.createDefaultModel();
        PropertyModelChangeProcessor.create(mModel, mView, TipsPromoViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
    }

    @SmallTest
    @Test
    public void testFeatureTipPromoData() {
        FeatureTipPromoData promoData = new FeatureTipPromoData(PROMO_TITLE, PROMO_DESCRIPTION);
        mModel.set(TipsPromoProperties.FEATURE_TIP_PROMO_DATA, promoData);

        assertEquals(PROMO_TITLE, mTitleView.getText());
        assertEquals(PROMO_DESCRIPTION, mDescriptionView.getText());
    }
}
