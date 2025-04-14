// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link BookmarkSearchBoxRow}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class BookmarkSearchBoxRowRenderTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private LinearLayout mContentView;
    private PropertyModel mPropertyModel;

    public BookmarkSearchBoxRowRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);

                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.bookmark_search_box_row, mContentView);

                    BookmarkSearchBoxRow bookmarkSearchBoxRow =
                            mContentView.findViewById(R.id.bookmark_toolbar);
                    mPropertyModel =
                            new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                                    .with(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES,
                                            R.string.price_tracking_bookmarks_filter_title)
                                    .with(
                                            BookmarkSearchBoxRowProperties
                                                    .SHOPPING_CHIP_START_ICON_RES,
                                            R.drawable.notifications_active)
                                    .with(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY,
                                            false)
                                    .build();

                    PropertyModelChangeProcessor.create(
                            mPropertyModel,
                            bookmarkSearchBoxRow,
                            BookmarkSearchBoxRowViewBinder.createViewBinder());
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal() throws IOException {
        mRenderTestRule.render(mContentView, "normal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testWithChip() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPropertyModel.set(
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, true));
        mRenderTestRule.render(mContentView, "withShoppingChip");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testWithSearchText() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(BookmarkSearchBoxRowProperties.SEARCH_TEXT, "foo");
                    mPropertyModel.set(
                            BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY,
                            true);
                });
        mRenderTestRule.render(mContentView, "searchText");
    }
}
