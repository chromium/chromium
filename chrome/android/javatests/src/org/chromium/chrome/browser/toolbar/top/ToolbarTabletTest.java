// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_TOOLBAR;

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
import org.chromium.base.test.util.DisabledTest;
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
@DisabledTest(message = "crbug.com/459863718")
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
}
