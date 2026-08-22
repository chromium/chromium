// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.core.widget.NestedScrollView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetController;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerJni;
import org.chromium.components.webapps.bottomsheet.PwaBottomSheetControllerProvider;

/** Integration tests for PWA Install bottom sheet UI and vertical scroll offset. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwaInstallBottomSheetIntegrationTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PwaBottomSheetController.Natives mNativeMock;

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;

    @Before
    public void setUp() throws Exception {
        PwaBottomSheetControllerJni.setInstanceForTesting(mNativeMock);
        WebPageStation unused = mActivityTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
                });
    }

    private void showPwaInstallBottomSheet() {
        runOnUiThreadBlocking(
                () -> {
                    PwaBottomSheetController controller =
                            PwaBottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    assertNotNull(controller);

                    Bitmap icon = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
                    icon.eraseColor(Color.BLUE);

                    controller.requestBottomSheetInstaller(
                            /* nativePwaBottomSheetController= */ 1L,
                            mActivityTestRule.getActivity().getWindowAndroid(),
                            mActivityTestRule.getActivity().getCurrentWebContents(),
                            icon,
                            /* isAdaptiveIcon= */ false,
                            "Test App",
                            "https://example.com",
                            "A test app description.");

                    mBottomSheetTestSupport.endAllAnimations();
                });
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testSheetShownAndElementsVisible() {
        showPwaInstallBottomSheet();

        onView(withText("Test App")).check(matches(isDisplayed()));
        onView(withText("https://example.com")).check(matches(isDisplayed()));
        onView(withId(R.id.button_install)).check(matches(isDisplayed()));
        onView(withId(R.id.drag_handlebar)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testVerticalScrollOffset() {
        showPwaInstallBottomSheet();

        BottomSheetContent content = mBottomSheetController.getCurrentSheetContent();
        assertNotNull(content);

        // Initially, the vertical scroll offset should be 0.
        assertEquals(0, content.getVerticalScrollOffset());

        NestedScrollView scrollView = content.getContentView().findViewById(R.id.scroll_view);
        assertNotNull(scrollView);

        // Add padding to create scrollable headroom and scroll the NestedScrollView.
        runOnUiThreadBlocking(
                () -> {
                    scrollView.setPadding(0, 0, 0, 500);
                    scrollView.setScrollY(50);
                });
        assertEquals(50, content.getVerticalScrollOffset());

        // Scroll back to top.
        runOnUiThreadBlocking(() -> scrollView.setScrollY(0));
        assertEquals(0, content.getVerticalScrollOffset());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testContentPriorityAndBasicProperties() {
        showPwaInstallBottomSheet();

        BottomSheetContent content = mBottomSheetController.getCurrentSheetContent();
        assertNotNull(content);

        assertTrue(content.swipeToDismissEnabled());
        assertEquals(
                R.string.pwa_install_bottom_sheet_accessibility,
                content.getSheetFullHeightAccessibilityStringId());
    }
}
