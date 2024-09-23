// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.page_info.ConnectionInfoView;
import org.chromium.content_public.browser.WebContents;

/** Tests for ConnectionInfoView. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(ConnectionInfoViewTest.PAGE_INFO_BATCH_NAME)
public class ConnectionInfoViewTest {
    public static final String PAGE_INFO_BATCH_NAME = "page_info";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    /** Tests that ConnectionInfoView can be instantiated and shown. */
    @Test
    @MediumTest
    @Feature({"ConnectionInfoView"})
    public void testShow() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeActivity context = sActivityTestRule.getActivity();
                    WebContents webContents = context.getActivityTab().getWebContents();
                    ConnectionInfoView.show(context, webContents, context.getModalDialogManager());
                });
    }
}
