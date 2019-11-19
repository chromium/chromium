// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/** Test suite for different Android URL schemes. */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UrlSchemeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String SIMPLE_SRC = "simple.html";
    private static final String SIMPLE_IMAGE = "google.png";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        TestContentProvider.resetResourceRequestCounts(InstrumentationRegistry.getTargetContext());
        TestContentProvider.setDataFilePath(
                InstrumentationRegistry.getTargetContext(), UrlUtils.getTestFilePath(""));
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test that resource request count in the content provider works.
     * This is to make sure that attempts to access the content provider
     * will be detected.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentProviderResourceRequestCount() throws IOException {
        String resource = SIMPLE_SRC;
        ensureResourceRequestCountInContentProvider(resource, 0);
        // Make a request to the content provider.
        Uri uri = Uri.parse(createContentUrl(resource));
        Context context = InstrumentationRegistry.getContext();
        InputStream inputStream = null;
        try {
            inputStream = context.getContentResolver().openInputStream(uri);
            Assert.assertNotNull(inputStream);
        } finally {
            if (inputStream != null) inputStream.close();
        }
        ensureResourceRequestCountInContentProvider(resource, 1);
    }

    /**
     * Make sure content URL access works.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlAccess() {
        String resource = SIMPLE_SRC;
        mActivityTestRule.loadUrl(createContentUrl(resource));
        ensureResourceRequestCountInContentProviderNotLessThan(resource, 1);
    }

    /**
     * Make sure a Content url *CANNOT* access the contents of an iframe that is loaded as a
     * content URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlIframeAccessFromContentUrl() throws Throwable {
        final String resource = "page_with_iframe_as_content_url.html";
        final String iframe = "simple_iframe.html";
        final String iframeId = "iframe_test_id";

        final String script = "var ifrm = document.getElementById('" + iframeId + "');"
                + "try {"
                + "  var a = ifrm.contentWindow.document.body.textContent;"
                + "} catch (e) {"
                + "  document.title = 'fail';"
                + "}";

        mActivityTestRule.loadUrl(createContentUrl(resource));

        // Make sure iframe is really loaded by verifying the title
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab().getTitle().equals(
                        "iframe loaded");
            }
        });
        // Make sure that content provider was asked to provide the content.
        ensureResourceRequestCountInContentProviderNotLessThan(iframe, 1);
        mActivityTestRule.runJavaScriptCodeInCurrentTab(script);

        // Make sure content access failed by verifying that title is set to fail.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab().getTitle().equals("fail");
            }
        });
    }

    /**
     * Test that a content URL is *ALLOWED* to access an image provided by a content URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlImageFromContentUrl() throws Throwable {
        verifyImageLoadRules(createContentUrl(SIMPLE_SRC), "success", 1);
    }

    /**
     * Test that a HTTP URL is *NOT ALLOWED* to access an image provided by a content URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlImageFromHttpUrl() throws Throwable {
        final String main = mTestServer.getURL("/chrome/test/data/android/" + SIMPLE_SRC);
        verifyImageLoadRules(main, "error", 0);
    }

    private void verifyImageLoadRules(String url, final String expectedTitle, int expectedLoadCount)
            throws Throwable {
        final String resource = SIMPLE_IMAGE;
        final String script = "var img = new Image();"
                + "  img.onerror = function() { document.title = 'error' };"
                + "  img.onabort = function() { document.title = 'error' };"
                + "  img.onload = function() { document.title = 'success' };"
                + "  img.src = '" + createContentUrl(resource) + "';"
                + "  document.body.appendChild(img);";
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.runJavaScriptCodeInCurrentTab(script);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab().getTitle().equals(
                        expectedTitle);
            }
        });
        ensureResourceRequestCountInContentProviderNotLessThan(resource, expectedLoadCount);
    }

    /**
     * Test that a content URL is not allowed within a data URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromData() {
        final String target = SIMPLE_IMAGE;
        mActivityTestRule.loadUrl(
                UrlUtils.encodeHtmlDataUri("<img src=\"" + createContentUrl(target) + "\">"));
        ensureResourceRequestCountInContentProvider(target, 0);
    }

    /**
     * Test that a content URL is not allowed within a local file.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromFile() throws IOException {
        final String target = SIMPLE_IMAGE;
        final File file = new File(Environment.getExternalStorageDirectory(), target + ".html");
        try {
            TestFileUtil.createNewHtmlFile(
                    file, target, "<img src=\"" + createContentUrl(target) + "\">");
            mActivityTestRule.loadUrl("file://" + file.getAbsolutePath());
            ensureResourceRequestCountInContentProvider(target, 0);
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    private String getTitleOnUiThread() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getActivityTab().getTitle());
    }

    /**
     * Test that the browser can be navigated to a file URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFileUrlNavigation() throws IOException {
        final File file = new File(Environment.getExternalStorageDirectory(),
                "url_navigation_test.html");

        try {
            TestFileUtil.createNewHtmlFile(file, "File", null);
            mActivityTestRule.loadUrl("file://" + file.getAbsolutePath());
            Assert.assertEquals("File", getTitleOnUiThread());
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = InstrumentationRegistry.getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        Assert.assertEquals(expectedCount, actualCount);
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedMinimalCount Expected minimal resource requests count
     */
    private void ensureResourceRequestCountInContentProviderNotLessThan(
            String resource, int expectedMinimalCount) {
        Context context = InstrumentationRegistry.getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        Assert.assertTrue("Minimal expected: " + expectedMinimalCount + ", actual: " + actualCount,
                actualCount >= expectedMinimalCount);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }
}
