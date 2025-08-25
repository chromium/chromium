// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID;

import android.app.Activity;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for PriceCardView. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
@DisabledTest(message = "https://crbug.com/424204696")
public class TabGridViewRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Activity mActivity;
    private TabGridView mTabGridView;
    private PropertyModel mModel;

    private final ThumbnailFetcher mNullThumbnailFetcher =
            new ThumbnailFetcher(
                    (tabId, thumbnailSize, isSelected, callback) -> callback.onResult(null),
                    MultiThumbnailMetadata.createMetadataWithoutUrls(
                            Tab.INVALID_TAB_ID, false, false, null));

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(1)
                    .build();

    public TabGridViewRenderTest(boolean isNightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(isNightModeEnabled);
        mRenderTestRule.setNightModeEnabled(isNightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(org.chromium.chrome.test.R.style.Theme_BrowserUI_DayNight);

        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup view = new FrameLayout(mActivity);
                    mActivity.setContentView(view, params);

                    mTabGridView =
                            (TabGridView)
                                    mActivity
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_grid_card_item, view, false);
                    view.addView(mTabGridView);

                    initModel();
                    PropertyModelChangeProcessor.create(
                            mModel, mTabGridView, TabGridViewBinder::bindTab);
                });
    }

    @After
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCardView_Highlighted() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridView.setIsHighlighted(TabCardHighlightState.HIGHLIGHTED, false));
        pollForHighlight(true);

        mRenderTestRule.render(mTabGridView, "tab_grid_view_highlight");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCardView_HighlightedIncognito() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridView.setIsHighlighted(TabCardHighlightState.HIGHLIGHTED, true));
        pollForHighlight(true);

        mRenderTestRule.render(mTabGridView, "tab_grid_view_highlight_incognito");
    }

    public void pollForHighlight(boolean isHighlighted) {
        View cardWrapper = mTabGridView.findViewById(R.id.card_wrapper);
        int neededVisibility = isHighlighted ? View.VISIBLE : View.INVISIBLE;
        float neededAlpha = isHighlighted ? 1.0f : 0.0f;
        CriteriaHelper.pollUiThread(
                () ->
                        cardWrapper.getVisibility() == neededVisibility
                                && cardWrapper.getAlpha() == neededAlpha);
    }

    public void initModel() {
        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, TabActionState.CLOSABLE)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.TAB_ID, Tab.INVALID_TAB_ID)
                        .with(TabProperties.IS_SELECTED, false)
                        .with(TabProperties.THUMBNAIL_FETCHER, mNullThumbnailFetcher)
                        .with(TabProperties.GRID_CARD_SIZE, new Size(500, 600))
                        .build();
    }
}
