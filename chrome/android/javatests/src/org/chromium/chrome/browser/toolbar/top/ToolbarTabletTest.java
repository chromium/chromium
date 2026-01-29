// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_TOOLBAR;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;

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
            RenderTestRule.Builder.withPublicCorpus().setBugComponent(UI_BROWSER_TOOLBAR).build();

    private ToolbarTablet mToolbar;
    private WebPageStation mPage;

    @BeforeClass
    public static void setupClass() {
        // Setting touch mode: false allows us to test the button's focused appearance.
        // It seems like touch mode has to be configured during setup.
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);
    }

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
    }

    @Test
    @SmallTest
    @Feature("RenderTest")
    public void testLastOmniboxButtonFocus_notClipped() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mToolbar.onUrlFocusChange(true));
        var bookmarkButton = mToolbar.findViewById(R.id.bookmark_button);
        ThreadUtils.runOnUiThreadBlocking(() -> bookmarkButton.requestFocus());
        mRenderTestRule.render(mToolbar, "last_button_focused");
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
}
