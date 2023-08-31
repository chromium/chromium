// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;

import java.util.List;

/**
 * Tests the management of multiple AwBrowserContexts (profiles)
 */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests focus on manipulation of global profile state")
public class MultiProfileTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwBrowserContext getContextSync(String name, boolean createIfNeeded) throws Throwable {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return AwBrowserContext.getNamedContext(name, createIfNeeded); });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateProfiles() throws Throwable {
        final AwBrowserContext nonDefaultProfile1 = getContextSync("1", true);
        final AwBrowserContext alsoNonDefaultProfile1 = getContextSync("1", true);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = getContextSync("Default", true);
        final AwBrowserContext nonDefaultProfile2 = getContextSync("2", true);

        Assert.assertNotNull(nonDefaultProfile1);
        Assert.assertNotNull(nonDefaultProfile2);
        Assert.assertNotNull(defaultProfile);
        Assert.assertSame(nonDefaultProfile1, alsoNonDefaultProfile1);
        Assert.assertSame(defaultProfile, defaultProfileByName);
        Assert.assertNotSame(nonDefaultProfile1, nonDefaultProfile2);
        Assert.assertNotSame(defaultProfile, nonDefaultProfile1);
        Assert.assertNotSame(nonDefaultProfile2, defaultProfile);

        final List<String> names =
                ThreadUtils.runOnUiThreadBlockingNoException(AwBrowserContext::listAllContexts);
        Assert.assertTrue(names.contains("1"));
        Assert.assertTrue(names.contains("2"));
        Assert.assertTrue(names.contains("Default"));
        Assert.assertFalse(names.contains("3"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetProfiles() throws Throwable {
        getContextSync("Exists", true);
        final AwBrowserContext existsProfile1 = getContextSync("Exists", false);
        final AwBrowserContext existsProfile2 = getContextSync("Exists", false);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = getContextSync("Default", false);
        final AwBrowserContext notExistsProfile = getContextSync("NotExists", false);

        Assert.assertNotNull(existsProfile1);
        Assert.assertNotNull(defaultProfile);
        Assert.assertNull(notExistsProfile);

        Assert.assertSame(existsProfile1, existsProfile2);
        Assert.assertSame(defaultProfile, defaultProfileByName);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCannotDeleteDefault() throws Throwable {
        mActivityTestRule.runOnUiThread(() -> {
            Assert.assertThrows(IllegalStateException.class,
                    () -> { AwBrowserContext.deleteNamedContext("Default"); });
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanDeleteNonExistent() throws Throwable {
        mActivityTestRule.runOnUiThread(
                () -> { Assert.assertFalse(AwBrowserContext.deleteNamedContext("DoesNotExist")); });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetName() throws Throwable {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext profile1 = getContextSync("AwesomeProfile", true);
        Assert.assertEquals("Default", defaultProfile.getName());
        Assert.assertEquals("AwesomeProfile", profile1.getName());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetRelativePath() throws Throwable {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = getContextSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile = getContextSync("MyOtherCoolProfile", true);
        Assert.assertEquals("Default", defaultProfile.getRelativePathForTesting());
        Assert.assertEquals("Profile 1", myCoolProfile.getRelativePathForTesting());
        Assert.assertEquals("Profile 2", myOtherCoolProfile.getRelativePathForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSharedPrefsNamesAreCorrectAndUnique() throws Throwable {
        final String dataDirSuffix = "MyDataDirSuffix";

        AwBrowserProcess.setProcessDataDirSuffixForTesting(dataDirSuffix);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = getContextSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile = getContextSync("MyOtherCoolProfile", true);
        final AwBrowserContext myCoolProfileCopy = getContextSync("MyCoolProfile", true);
        Assert.assertEquals("WebViewProfilePrefsDefault_MyDataDirSuffix",
                defaultProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals("WebViewProfilePrefsProfile 1_MyDataDirSuffix",
                myCoolProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals("WebViewProfilePrefsProfile 2_MyDataDirSuffix",
                myOtherCoolProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals(myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());

        AwBrowserProcess.setProcessDataDirSuffixForTesting(null);
        Assert.assertEquals(
                "WebViewProfilePrefsDefault", defaultProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals(
                "WebViewProfilePrefsProfile 1", myCoolProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals(
                "WebViewProfilePrefsProfile 2", myOtherCoolProfile.getSharedPrefsNameForTesting());
        Assert.assertEquals(myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());
    }
}
