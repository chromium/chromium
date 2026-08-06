// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
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
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/** Render tests for the desktop android bookmark popup view. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class BookmarkPopupViewRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .setRevision(1)
                    .setDescription("Remove duplicate popup background")
                    .build();

    private BookmarkPopupView mView;

    public BookmarkPopupViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout contentView = new LinearLayout(activity);
                    contentView.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(activity));
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);
                    activity.setContentView(contentView, params);

                    mView =
                            (BookmarkPopupView)
                                    LayoutInflater.from(activity)
                                            .inflate(R.layout.bookmark_popup, contentView, false);
                    contentView.addView(mView);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkPopupView() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setHeaderText("Bookmark added");
                    mView.setTitle("Test Bookmark");
                    mView.setFolderName("Mobile bookmarks");
                });
        mRenderTestRule.render(mView, "bookmark_popup_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkPopupView_PriceTrackingUnchecked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setHeaderText("Bookmark added");
                    mView.setTitle("Test Bookmark");
                    mView.setFolderName("Mobile bookmarks");
                    mView.setPriceTrackingVisible(true);
                    mView.setPriceTrackingEnabled(true);
                    mView.setPriceTrackingSwitchChecked(false);
                });
        mRenderTestRule.render(mView, "bookmark_popup_view_price_tracking_unchecked");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkPopupView_PriceTrackingChecked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setHeaderText("Bookmark added");
                    mView.setTitle("Test Bookmark");
                    mView.setFolderName("Mobile bookmarks");
                    mView.setPriceTrackingVisible(true);
                    mView.setPriceTrackingEnabled(true);
                    mView.setPriceTrackingSwitchChecked(true);
                });
        mRenderTestRule.render(mView, "bookmark_popup_view_price_tracking_checked");
    }
}
