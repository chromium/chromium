// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_TOOLBAR;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Instrumentation tests for {@link ToolbarTablet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class ToolbarTabletTest {
    @ClassRule
    public static AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(UI_BROWSER_TOOLBAR)
                    .build();

    private ToolbarTablet mToolbar;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
    }

    @Test
    @SmallTest
    @Feature("RenderTest")
    @DisableFeatures(ChromeFeatureList.HOME_BUTTON_REMOVAL)
    public void testLastOmniboxButtonFocus_notClipped() throws IOException {
        testLastOmniboxButtonFocus_notClippedImpl("last_button_focused");
    }

    @Test
    @SmallTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    public void testLastOmniboxButtonFocus_notClipped_withHomeButtonRemovalKeepOnNtp()
            throws IOException {
        testLastOmniboxButtonFocus_notClippedImpl("last_button_focused_with_home_button_removal");
    }

    private void testLastOmniboxButtonFocus_notClippedImpl(String goldenId) throws IOException {
        // Transition to URL focused state, which expands the Omnibox on tablets.
        ThreadUtils.runOnUiThreadBlocking(() -> mToolbar.onUrlFocusChange(true));

        var bookmarkButton = mToolbar.findViewById(R.id.bookmark_button);

        // Wait for the button to be visible and then request focus. We explicitly set
        // focusableInTouchMode to true because the system-wide touch mode state is often
        // unpredictable in instrumentation tests.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Bookmark button is not visible",
                            bookmarkButton.getVisibility(),
                            is(View.VISIBLE));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bookmarkButton.setFocusableInTouchMode(true);
                    bookmarkButton.requestFocus();
                });

        // Wait for focus to be acquired and for the layout to stabilize (ensures the focus ripple
        // has finished its initial draw).
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Bookmark button should be focused",
                            bookmarkButton.isFocused(),
                            is(true));
                });
        ViewUtils.waitForStableView(mToolbar);

        mRenderTestRule.render(mToolbar, goldenId);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    public void testToolbarButtonDimensionsAndStyles() {
        // Verify the home button as a representative sample; all toolbar buttons should share the
        // same style and dimensions.
        View homeButton = mToolbar.findViewById(R.id.home_button);
        int toolbarHeight = mToolbar.getHeight();

        int width =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        int height =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_height);
        int marginHorizontal =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_margin_horizontal);
        int marginVertical =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_margin_vertical);

        // Verify button dimensions.
        assertEquals(width, homeButton.getWidth());
        assertEquals(height, homeButton.getHeight());

        // Verify that the vertical margins correctly center the button within its parent container.
        assertEquals((toolbarHeight - height) / 2, marginVertical);

        // Verify button margins.
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) homeButton.getLayoutParams();
        assertEquals("Start margin mismatch", marginHorizontal, lp.getMarginStart());
        assertEquals("End margin mismatch", marginHorizontal, lp.getMarginEnd());
        assertEquals("Top margin mismatch", marginVertical, lp.topMargin);
        assertEquals("Bottom margin mismatch", marginVertical, lp.bottomMargin);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @Feature("TAB STRIP DENSITY CHANGE")
    public void testToolbarButtonDimensionsAndStyles_desktop() {
        // Verify the home button as a representative sample; all toolbar buttons should share the
        // same style and dimensions.
        View homeButton = mToolbar.findViewById(R.id.home_button);
        int toolbarHeight = mToolbar.getHeight();

        int width =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width_desktop);
        int height =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_height_desktop);
        int marginHorizontal =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_margin_horizontal_desktop);
        int marginVertical =
                homeButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_margin_vertical_desktop);

        // Verify button dimensions. The button dimensions are reduced on Desktop compared to
        // Tablet.
        assertEquals(width, homeButton.getWidth());
        assertEquals(height, homeButton.getHeight());

        // Verify that the vertical margins correctly center the button within its parent container.
        assertEquals((toolbarHeight - height) / 2, marginVertical);

        // Verify button margins. These margins are applied on desktop to compensate for the smaller
        // button size, ensuring the buttons maintain their relative positioning.
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) homeButton.getLayoutParams();
        assertEquals("Start margin mismatch", marginHorizontal, lp.getMarginStart());
        assertEquals("End margin mismatch", marginHorizontal, lp.getMarginEnd());
        assertEquals("Top margin mismatch", marginVertical, lp.topMargin);
        assertEquals("Bottom margin mismatch", marginVertical, lp.bottomMargin);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    public void testOmniboxButtonDimensionsAndStyles() {
        // Verify the bookmark button as a representative sample; all omnibox buttons should share
        // the same style and dimensions.
        View bookmarkButton = mToolbar.findViewById(R.id.bookmark_button);

        int width =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        int height =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_height);
        int marginHorizontal =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_margin_horizontal);

        // Verify button dimensions.
        assertEquals(width, bookmarkButton.getWidth());
        assertEquals(height, bookmarkButton.getHeight());

        // Verify button margin.
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) bookmarkButton.getLayoutParams();
        assertEquals("Start margin mismatch", marginHorizontal, lp.getMarginStart());
        assertEquals("End margin mismatch", marginHorizontal, lp.getMarginEnd());
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @Feature("TAB STRIP DENSITY CHANGE")
    public void testOmniboxButtonDimensionsAndStyles_desktop() {
        // Verify the bookmark button as a representative sample; all omnibox buttons should share
        // the same style and dimensions.
        View bookmarkButton = mToolbar.findViewById(R.id.bookmark_button);

        int width =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width_desktop);
        int height =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_height_desktop);
        int marginHorizontal =
                bookmarkButton
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.location_bar_action_icon_margin_horizontal_desktop);

        // Verify button dimensions. The button dimensions are reduced on Desktop compared to
        // Tablet.
        assertEquals(width, bookmarkButton.getWidth());
        assertEquals(height, bookmarkButton.getHeight());

        // Verify button margins. These margins are applied on desktop to compensate for the smaller
        // button size, ensuring the buttons maintain their relative positioning.
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) bookmarkButton.getLayoutParams();
        assertEquals("Start margin mismatch", marginHorizontal, lp.getMarginStart());
        assertEquals("End margin mismatch", marginHorizontal, lp.getMarginEnd());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SNAPSHOT_REFACTOR)
    public void testToolbarSnapshotRefactorFlagEnabled() {
        int expectedToolbarHeight =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        // Capture live view locations directly off the active UI hierarchy tree.
        ToolbarControlContainer controlContainer =
                mActivityTestRule.getActivity().findViewById(R.id.control_container);
        View toolbarContainer =
                mActivityTestRule.getActivity().findViewById(R.id.toolbar_container);
        View hairline = mActivityTestRule.getActivity().findViewById(R.id.toolbar_hairline);
        View toolbarView = mActivityTestRule.getActivity().findViewById(R.id.toolbar);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Extract the exact runtime value of the tab strip height.
                    int expectedTabStripHeight =
                            controlContainer.getMeasuredHeight()
                                    - controlContainer.getControlContainerHeightExcludingTabStrip();

                    MarginLayoutParams toolbarContainerParams =
                            (MarginLayoutParams) toolbarContainer.getLayoutParams();
                    MarginLayoutParams hairlineParams =
                            (MarginLayoutParams) hairline.getLayoutParams();
                    MarginLayoutParams toolbarParams =
                            (MarginLayoutParams) toolbarView.getLayoutParams();

                    assertEquals(
                            "Toolbar container top margin should be the tab strip height.",
                            expectedTabStripHeight,
                            toolbarContainerParams.topMargin);

                    assertEquals(
                            "Hairline top margin should be the toolbar height.",
                            expectedToolbarHeight,
                            hairlineParams.topMargin);

                    assertEquals(
                            "Inner toolbar view top margin should be stripped down to 0 under the"
                                    + " snapshot refactor.",
                            0,
                            toolbarParams.topMargin);
                });
    }
}
