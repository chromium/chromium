// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Base64;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for the Default Offline behavior when loading a TWA (and failing to). */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappDefaultOfflineTwaTest {
    // The actual packageName we use, when trying to load the TWA, doesn't actually matter, because
    // loading is blocked via `interceptWithOfflineError`. However, the package must exist so that
    // lookup functions don't throw an error. Therefore, we use the test bundle as package name.
    private static final String TWA_PACKAGE_NAME = "org.chromium.chrome.tests";

    // Likewise, the value of this doesn't matter a great deal because the loading is intercepted,
    // but we have to specify something.
    private static final String TEST_PATH = "/chrome/test/data/android/google.html";

    // The values we look for in the test.
    private static final String TWA_NAME = "shortname";
    private static final int TWA_BACKGROUND_COLOR = 0x00FF00;

    private EmbeddedTestServer mTestServer;
    private TestContext mTestContext;

    private static BitmapDrawable getTestIconDrawable(Resources resources, String imageAsString) {
        byte[] bytes = Base64.decode(imageAsString.getBytes(), Base64.DEFAULT);
        BitmapDrawable bitmapDrawable =
                new BitmapDrawable(
                        resources, BitmapFactory.decodeByteArray(bytes, 0, bytes.length));
        return bitmapDrawable;
    }

    private static class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public CharSequence getApplicationLabel(ApplicationInfo info) {
                    if (!TWA_PACKAGE_NAME.equals(info.packageName)) {
                        return super.getApplicationLabel(info);
                    }

                    return TWA_NAME;
                }

                @Override
                public Drawable getApplicationIcon(String packageName)
                        throws NameNotFoundException {
                    if (!TWA_PACKAGE_NAME.equals(packageName)) {
                        return super.getApplicationIcon(packageName);
                    }

                    return getTestIconDrawable(getResources(), WebappActivityTestRule.TEST_ICON);
                }
            };
        }
    }

    @Before
    public void setUp() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        // Setup the context for our custom PackageManager.
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);
    }

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private void launchTwa(String twaPackageName, String url, boolean withAssetLinkVerification)
            throws TimeoutException {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, TWA_BACKGROUND_COLOR);
        if (withAssetLinkVerification) {
            TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        }
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    public void testDefaultOfflineTwa(boolean withAssetLinkVerification) throws Exception {
        mCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mCustomTabActivityTestRule.getTestServer();

        final String testAppUrl = mTestServer.getURL(TEST_PATH);
        OfflineTestUtil.interceptWithOfflineError(testAppUrl);

        launchTwa(TWA_PACKAGE_NAME, testAppUrl, withAssetLinkVerification);

        // Ensure that web_app_default_offline.html is showing the correct values.
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertEquals(
                "\"shortname\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.title;"));
        assertEquals(
                "\"You're offline\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(),
                        "document.getElementById('default-web-app-msg').textContent;"));

        String imageAsString =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.getElementById('icon').src;");
        // Remove the base64 prefix and convert the line-feeds (%0A) so that the strings can be
        // compared.
        imageAsString =
                imageAsString.substring(
                        "\"data:image/png;base64,".length(), imageAsString.length() - 1);
        imageAsString = imageAsString.replaceAll("%0A", "\n");

        BitmapDrawable expectedDrawable =
                getTestIconDrawable(
                        mCustomTabActivityTestRule.getActivity().getResources(),
                        WebappActivityTestRule.TEST_ICON);
        String expectedString =
                BitmapHelper.encodeBitmapAsString(expectedDrawable.getBitmap()).trim();
        assertTrue(imageAsString.equals(expectedString));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDefaultOfflineTwaWithoutVerification() throws Exception {
        // Test default offline behavior without asset link verification, which causes the app to
        // run in CCT (and is what happens when TWAs load for the first time without network
        // connectivity, because no cached results are available).
        testDefaultOfflineTwa(false); // Run without asset link verification.
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDefaultOfflineTwaWithVerification() throws Exception {
        testDefaultOfflineTwa(true); // Run with asset link verification.
    }
}
