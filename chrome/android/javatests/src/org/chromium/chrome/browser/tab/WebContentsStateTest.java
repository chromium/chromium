// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;

/** Tests whether TabState can be restored from disk properly. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebContentsStateTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();
    private TestTabModelDirectory mTestTabModelDirectory;
    private Profile mProfile;

    @Before
    public void setUp() {
        mTestTabModelDirectory =
                new TestTabModelDirectory(
                        ApplicationProvider.getApplicationContext(), "WebContentsStateTest", null);
        mProfile = ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
    }

    @After
    public void tearDown() {
        mTestTabModelDirectory.tearDown();
    }

    private static void writeFile(File directory, String filename, byte[] data) throws Exception {
        File file = new File(directory, filename);
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(data);
        } catch (FileNotFoundException e) {
            throw new AssertionError("Failed to create " + filename);
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    /** Tests that Chrome doesn't crash parsing a corrupted tab state. crbug/1094239 */
    @Test
    @SmallTest
    public void testLoadCorruptedTabState() throws Exception {
        writeFile(
                mTestTabModelDirectory.getBaseDirectory(),
                "tab0",
                new byte[] {
                    0, 0, 0, 0, 0, 0, 0, 0, // encryption key
                    0, 0, 0, 0, 0, 0, 0, 0, // timestamp
                    0, 0, 0, 0, // length is 0 - (i.e. map 0-length content state)
                    'g', 'a', 'r', 'b', 'a', 'g', 'e'
                });

        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), "tab0");
        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(
                        tabStateFile, false, new CipherFactory());
        // Garbage-in, garbage out. Client code must be tolerant to null TabState
        Assert.assertNotNull(tabState);
        Assert.assertNotNull(tabState.contentsState);
        Assert.assertNotNull(tabState.contentsState.buffer());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Return a null contents state but don't crash.
                    Assert.assertNull(tabState.contentsState.restoreWebContents(mProfile, false));
                    tabState.contentsState.destroy();
                });
    }
}
