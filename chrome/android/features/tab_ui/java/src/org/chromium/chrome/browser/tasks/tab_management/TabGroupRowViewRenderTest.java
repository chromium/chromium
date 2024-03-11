// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.START_DRAWABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.core.util.Pair;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.time.Clock;

/** Render tests for {@link TabGroupRowView}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabGroupRowViewRenderTest {
    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .build();

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TestThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);
    }

    private void setUpOnUi() {
        mTabGroupRowView = inflateAndAttach(mActivity, R.layout.tab_group_row);
    }

    private <T extends View> T inflateAndAttach(Context context, @LayoutRes int layoutRes) {
        FrameLayout contentView = new FrameLayout(mActivity);
        contentView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mActivity.setContentView(contentView);

        LayoutInflater inflater = LayoutInflater.from(context);
        inflater.inflate(layoutRes, contentView);
        assert contentView.getChildCount() == 1;
        return (T) contentView.getChildAt(0);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithStartDrawable() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(START_DRAWABLE, new ColorDrawable(Color.GREEN));
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(TITLE_DATA, new Pair<>("Title", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());
                });
        mRenderTestRule.render(mTabGroupRowView, "with");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithoutStartDrawable() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(TITLE_DATA, new Pair<>("Title", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());
                });
        mRenderTestRule.render(mTabGroupRowView, "without");
    }
}
