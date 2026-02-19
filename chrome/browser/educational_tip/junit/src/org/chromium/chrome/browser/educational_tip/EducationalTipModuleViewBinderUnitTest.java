// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_COMPLETED_IMAGE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_IMAGE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

/** Tests for {@link EducationalTipModuleViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class EducationalTipModuleViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    private EducationalTipModuleView mEducationalTipModuleView;
    @Mock private EducationalTipModuleView mMockView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    @Mock private View.OnClickListener mModuleButtonOnClickListener;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mEducationalTipModuleView =
                (EducationalTipModuleView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.educational_tip_module_layout, null);
        mActivity.setContentView(mEducationalTipModuleView);
        mModel = new PropertyModel(EducationalTipModuleProperties.ALL_KEYS);
    }

    @After
    public void tearDown() throws Exception {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }
        mModel = null;
        mEducationalTipModuleView = null;
        mActivity = null;
    }

    @Test
    @SmallTest
    public void testSetModuleContentTitle() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        TextView contentTitleView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_title);
        assertEquals("", contentTitleView.getText());

        String expectedTitle =
                mActivity.getString(
                        org.chromium.chrome.browser.educational_tip.R.string.use_chrome_by_default);
        mModel.set(MODULE_CONTENT_TITLE_STRING, expectedTitle);
        Assert.assertEquals(expectedTitle, contentTitleView.getText());
    }

    @Test
    @SmallTest
    public void testSetModuleContentDescription() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        TextView contentDescriptionView =
                mEducationalTipModuleView.findViewById(
                        R.id.educational_tip_module_content_description);
        assertEquals("", contentDescriptionView.getText());

        String expectedTitle =
                mActivity.getString(
                        org.chromium.chrome.browser.educational_tip.R.string
                                .educational_tip_default_browser_description);
        mModel.set(MODULE_CONTENT_DESCRIPTION_STRING, expectedTitle);
        Assert.assertEquals(expectedTitle, contentDescriptionView.getText());
    }

    @Test
    @SmallTest
    public void testSetModuleContentImage() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleViewBinder::bind);
        int expectedRes =
                org.chromium.chrome.browser.educational_tip.R.drawable.default_browser_promo_logo;
        mModel.set(MODULE_CONTENT_IMAGE, expectedRes);
        verify(mMockView).setContentImageResource(expectedRes);
    }

    @Test
    @SmallTest
    public void testSetModuleContentCompletedImage() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleViewBinder::bind);
        int expectedRes = R.drawable.setup_list_completed_background_wavy_circle;
        mModel.set(MODULE_CONTENT_COMPLETED_IMAGE, expectedRes);
        verify(mMockView).setContentImageResourceWithAnimation(expectedRes);
    }

    @Test
    @SmallTest
    public void testMarkCompleted() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleViewBinder::bind);
        mModel.set(MARK_COMPLETED, true);
        verify(mMockView).setCompleted(true);
    }

    @Test
    @SmallTest
    public void testSetModuleButtonOnClickListener() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        mModel.set(MODULE_BUTTON_ON_CLICK_LISTENER, mModuleButtonOnClickListener);
        ButtonCompat moduleButtonView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_button);
        assertNotNull(moduleButtonView);
        moduleButtonView.performClick();
        verify(mModuleButtonOnClickListener).onClick(any());
    }
}
