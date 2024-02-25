// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.storage;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.io.File;
import java.io.FileWriter;

/** Integration test suite for the BlobUrlStore. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BlobUrlStoreTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

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
        mActivityTestRule.startMainActivityWithURL(
                TestContentProvider.createContentUrl("blob.html"));
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals("A blob", ChromeTabUtils.getTitleOnUiThread(tab));
        Assert.assertTrue(ChromeTabUtils.getUrlStringOnUiThread(tab).startsWith("blob:null/"));
    }
}
