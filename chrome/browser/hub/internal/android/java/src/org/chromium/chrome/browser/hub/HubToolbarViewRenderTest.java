// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Render tests for {@link HubPaneHostView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class HubToolbarViewRenderTest {
    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    private Activity mActivity;
    private HubToolbarView mToolbar;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TestThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);
    }

    private void setUpOnUi() {
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mToolbar = (HubToolbarView) inflater.inflate(R.layout.hub_toolbar_layout, null, false);
        mActivity.setContentView(mToolbar);

        mPropertyModel = new PropertyModel(HubToolbarProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mToolbar, HubToolbarViewBinder::bind);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActionButton() throws Exception {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.new_tab_icon);
        FullButtonData fullButtonData = new DelegateButtonData(displayButtonData, () -> {});

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, fullButtonData);
                    mPropertyModel.set(HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT, true);
                });
        mRenderTestRule.render(mToolbar, "actionButtonWithText");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT, false));
        mRenderTestRule.render(mToolbar, "actionButtonOnlyImage");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, null));
        mRenderTestRule.render(mToolbar, "noActionButton");
    }
}
