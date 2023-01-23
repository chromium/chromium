// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Test that verifies back press will dismiss the selection popup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class SelectionPopupBackPressTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head>"
            + "  <meta name=viewport content='width=device-width, initial-scale=1.0'>"
            + "</head>"
            + "<body>"
            + "<p id=\"selection_popup_text\">Test</p>"
            + "</body>"
            + "</html>");

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressClearSelection() throws TimeoutException {
        testBackPressClearSelectionInternal();
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressClearSelection_BackPressRefactor() throws TimeoutException {
        testBackPressClearSelectionInternal();
    }

    private void testBackPressClearSelectionInternal() throws TimeoutException {
        mActivityTestRule.startMainActivityWithURL(TEST_PAGE);
        DOMUtils.longPressNodeByJs(mActivityTestRule.getWebContents(),
                "document.getElementById('selection_popup_text')");
        SelectionPopupController controller =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return SelectionPopupController.fromWebContents(
                            mActivityTestRule.getWebContents());
                });
        Assert.assertNotNull(controller);
        // Wait until popup has been displayed.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Selection popup should be triggered after long press",
                    controller.isSelectActionBarShowing(), Matchers.is(true));
        });
        Assert.assertTrue(
                "Selection popup should be triggered after long press.", controller.hasSelection());
        Assert.assertTrue("Selection popup should be triggered after long press.",
                controller.isSelectActionBarShowingSupplier().get());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BackPressManager backPressManager =
                    mActivityTestRule.getActivity().getBackPressManagerForTesting();
            if (backPressManager.has(BackPressHandler.Type.TEXT_BUBBLE)) {
                mActivityTestRule.getActivity().getBackPressManagerForTesting().removeHandler(
                        BackPressHandler.Type.TEXT_BUBBLE);
            }
        });

        Espresso.pressBack();

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Selection popup should be dismissed after long press",
                    controller.isSelectActionBarShowing(), Matchers.is(false));
        });
        Assert.assertFalse(
                "Selection popup should be dismissed on back press.", controller.hasSelection());
        Assert.assertFalse("Selection popup should be dismissed on back press.",
                controller.isSelectActionBarShowingSupplier().get());
    }
}
