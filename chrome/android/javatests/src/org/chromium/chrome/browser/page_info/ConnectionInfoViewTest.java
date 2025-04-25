// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.page_info.ConnectionInfoView;
import org.chromium.content_public.browser.WebContents;

/** Tests for ConnectionInfoView. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ConnectionInfoViewTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    /** Tests that ConnectionInfoView can be instantiated and shown. */
    @Test
    @MediumTest
    @Feature({"ConnectionInfoView"})
    public void testShow() throws InterruptedException {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        WebContents webContents = page.webContentsElement.get();
        ChromeTabbedActivity activity = page.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ConnectionInfoView.show(
                                activity, webContents, activity.getModalDialogManager()));
    }
}
