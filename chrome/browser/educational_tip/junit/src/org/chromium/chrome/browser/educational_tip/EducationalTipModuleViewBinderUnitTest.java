// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_IMAGE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
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
    private Activity mActivity;
    private EducationalTipModuleView mEducationalTipModuleView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    @Mock private View.OnClickListener mModuleButtonOnClickListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mEducationalTipModuleView =
                (EducationalTipModuleView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.educational_tip_module_layout, null);
        mActivity.setContentView(mEducationalTipModuleView);
        mModel = new PropertyModel(EducationalTipModuleProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mEducationalTipModuleView, EducationalTipModuleViewBinder::bind);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mModel = null;
        mEducationalTipModuleView = null;
        mActivity = null;
    }

    @Test
    @SmallTest
    public void testSetModuleContentTitle() {
        TextView contentTitleView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_title);
        assertEquals("", contentTitleView.getText());

        String expectedTitle =
                mActivity.getString(
                        org.chromium.chrome.browser.educational_tip.R.string
                                .educational_tip_default_browser_title);
        mModel.set(MODULE_CONTENT_TITLE_STRING, expectedTitle);
        Assert.assertEquals(expectedTitle, contentTitleView.getText());
    }

    @Test
    @SmallTest
    public void testSetModuleContentDescription() {
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
        ImageView contentImageView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_content_image);
        assertNull(contentImageView.getDrawable());

        mModel.set(
                MODULE_CONTENT_IMAGE,
                org.chromium.chrome.browser.educational_tip.R.drawable.default_browser_promo_logo);
        assertNotNull(contentImageView.getDrawable());
    }

    @Test
    @SmallTest
    public void testSetModuleButtonOnClickListener() {
        mModel.set(MODULE_BUTTON_ON_CLICK_LISTENER, mModuleButtonOnClickListener);
        ButtonCompat moduleButtonView =
                mEducationalTipModuleView.findViewById(R.id.educational_tip_module_button);
        assertNotNull(moduleButtonView);
        moduleButtonView.performClick();
        verify(mModuleButtonOnClickListener).onClick(any());
    }
}
