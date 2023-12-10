// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.ColorInt;
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
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link HubPaneHostView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HubPaneHostViewRenderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    private Activity mActivity;
    private HubPaneHostView mPaneHost;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TestThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);
    }

    private void setUpOnUi() {
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mPaneHost = (HubPaneHostView) inflater.inflate(R.layout.hub_pane_host_layout, null, false);
        mActivity.setContentView(mPaneHost);

        mPropertyModel = new PropertyModel(HubPaneHostProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mPaneHost, HubPaneHostViewBinder::bind);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void test() throws Exception {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        FullButtonData fullButtonData = new DelegateButtonData(displayButtonData, () -> {});
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @ColorInt int defaultBgColor = SemanticColorUtils.getDefaultBgColor(mActivity);
                    View rootView = solidColorView(defaultBgColor);
                    mPropertyModel.set(PANE_ROOT_VIEW, rootView);
                    mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
                });
        mRenderTestRule.render(mPaneHost, "greenAndButton");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(PANE_ROOT_VIEW, null);
                    mPropertyModel.set(ACTION_BUTTON_DATA, null);
                });
        mRenderTestRule.render(mPaneHost, "null");
    }

    private View solidColorView(@ColorInt int color) {
        View view = new View(mPaneHost.getContext());
        view.setBackgroundColor(color);
        return view;
    }
}
