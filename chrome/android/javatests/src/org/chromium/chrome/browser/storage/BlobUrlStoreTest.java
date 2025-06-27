// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.storage;

import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.io.File;
import java.io.FileWriter;

/** Integration test suite for the BlobUrlStore. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BlobUrlStoreTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public TemporaryFolder mFolder = new TemporaryFolder();

    @Test
    @SmallTest
    public void testContentScheme() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        FileWriter writer = new FileWriter(new File(mFolder.newFolder("android"), "blob.html"));
        writer.write(
                "<!DOCTYPE html>"
                        + "<html>"
                        + "  <body>"
                        + "    <a href='#' id='link'>click me</a>"
                        + "    <script>"
                        + "      let blob = new Blob(['<html>"
                        + "            <head><title>A blob</title></head>"
                        + "            <body>some text</body>"
                        + "          </html>'], {type: 'text/html'});"
                        + "      link.href = URL.createObjectURL(blob);"
                        + "      link.click()"
                        + "    </script>"
                        + "  </body>"
                        + "</html>");
        writer.close();
        TestContentProvider.resetResourceRequestCounts(context);
        TestContentProvider.setDataFilePath(context, mFolder.getRoot().getPath());
        WebPageStation page =
                mActivityTestRule
                        .startOnUrlTo(TestContentProvider.createContentUrl("blob.html"))
                        .arriveAt(
                                WebPageStation.newBuilder()
                                        .withEntryPoint()
                                        .withExpectedTitle("A blob")
                                        .withExpectedUrlSubstring("blob:null/")
                                        .build());
        assertTrue(ChromeTabUtils.getUrlStringOnUiThread(page.getTab()).startsWith("blob:null/"));
    }
}
