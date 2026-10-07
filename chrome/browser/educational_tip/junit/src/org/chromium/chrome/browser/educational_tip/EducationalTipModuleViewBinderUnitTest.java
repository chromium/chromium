// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_COMPLETED_IMAGE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_IMAGE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.USE_TRANSPARENT_ICON_BACKGROUND;

import android.app.Activity;
import android.graphics.Paint;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

/** Tests for {@link EducationalTipModuleViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class EducationalTipModuleViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    private EducationalTipModuleView mEducationalTipModuleView;
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
    public void testSetModuleContentImage() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        ImageView imageView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_image);
        imageView.setAlpha(0.5f);
        int expectedRes =
                org.chromium.chrome.browser.educational_tip.R.drawable.default_browser_promo_logo;
        mModel.set(MODULE_CONTENT_IMAGE, expectedRes);
        assertEquals(expectedRes, shadowOf(imageView.getDrawable()).getCreatedFromResId());
        assertEquals(1f, imageView.getAlpha(), 0f);
    }

    @Test
    public void testSetModuleContentCompletedImage() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        ImageView imageView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_image);
        int expectedRes = R.drawable.setup_list_completed_background_wavy_circle;
        mModel.set(MODULE_CONTENT_COMPLETED_IMAGE, expectedRes);
        // The image is swapped once the fade-out animation ends.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(expectedRes, shadowOf(imageView.getDrawable()).getCreatedFromResId());
        assertEquals(1f, imageView.getAlpha(), 0f);
    }

    @Test
    public void testMarkCompleted() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        TextView contentTitleView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_title);
        ButtonCompat moduleButtonView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_button);
        assertTrue(moduleButtonView.isEnabled());

        mModel.set(MARK_COMPLETED, true);
        assertTrue((contentTitleView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG) != 0);
        assertFalse(moduleButtonView.isEnabled());
    }

    @Test
    public void testSetUseTransparentIconBackground() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
        ImageView imageView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_image);
        assertNotNull(imageView.getBackground());
        mModel.set(USE_TRANSPARENT_ICON_BACKGROUND, true);
        assertNull(imageView.getBackground());
    }

    @Test
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
