// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URLEncoder;

/** Test suite for different Android URL schemes. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UrlSchemeTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String SIMPLE_SRC = "simple.html";
    private static final String SIMPLE_IMAGE = "google.png";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        TestContentProvider.resetResourceRequestCounts(ApplicationProvider.getApplicationContext());
        TestContentProvider.setDataFilePath(
                ApplicationProvider.getApplicationContext(), UrlUtils.getTestFilePath(""));
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    /**
     * Test that resource request count in the content provider works. This is to make sure that
     * attempts to access the content provider will be detected.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentProviderResourceRequestCount() throws IOException {
        String resource = SIMPLE_SRC;
        ensureResourceRequestCountInContentProvider(resource, 0);
        // Make a request to the content provider.
        Uri uri = Uri.parse(createContentUrl(resource));
        Context context = ApplicationProvider.getApplicationContext();
        InputStream inputStream = null;
        try {
            inputStream = context.getContentResolver().openInputStream(uri);
            Assert.assertNotNull(inputStream);
        } finally {
            if (inputStream != null) inputStream.close();
        }
        ensureResourceRequestCountInContentProvider(resource, 1);
    }

    /** Make sure content URL access works. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlAccess() {
        String resource = SIMPLE_SRC;
        sActivityTestRule.loadUrl(createContentUrl(resource));
        ensureResourceRequestCountInContentProviderNotLessThan(resource, 1);
    }

    /**
     * Make sure a Content url *CANNOT* access the contents of an iframe that is loaded as a content
     * URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlIframeAccessFromContentUrl() throws Throwable {
        final String resource = "page_with_iframe_as_content_url.html";
        final String iframe = "simple_iframe.html";
        final String iframeId = "iframe_test_id";

        final String script =
                "var ifrm = document.getElementById('"
                        + iframeId
                        + "');"
                        + "try {"
                        + "  var a = ifrm.contentWindow.document.body.textContent;"
                        + "} catch (e) {"
                        + "  document.title = 'fail';"
                        + "}";

        sActivityTestRule.loadUrl(createContentUrl(resource));

        // Make sure iframe is really loaded by verifying the title
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getTitleOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is("iframe loaded"));
                });
        // Make sure that content provider was asked to provide the content.
        ensureResourceRequestCountInContentProviderNotLessThan(iframe, 1);
        sActivityTestRule.runJavaScriptCodeInCurrentTab(script);

        // Make sure content access failed by verifying that title is set to fail.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getTitleOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is("fail"));
                });
    }

    private String loadContentUrlToMakeCorsToContent(final String api, final String mode)
            throws Throwable {
        final String resource = "content_url_make_cors_to_content.html";
        final String imageUrl = createContentUrl("google.png");

        sActivityTestRule.loadUrl(
                createContentUrl(resource)
                        + "?api="
                        + api
                        + "&mode="
                        + mode
                        + "&url="
                        + URLEncoder.encode(imageUrl));

        // Make sure the CORS request fail in the page.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getTitleOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.not("running"));
                });

        // Make sure that content provider was asked to provide the content.
        ensureResourceRequestCountInContentProviderNotLessThan(resource, 1);

        return ChromeTabUtils.getTitleOnUiThread(sActivityTestRule.getActivity().getActivityTab());
    }

    @Test
    @MediumTest
    @Feature({"CORS"})
    public void testContentUrlCanNotMakeXhrRequestToContentUrl() throws Throwable {
        // The XMLHttpRequest can carry content:// URLs, but CORS requests for content:// are not
        // permitted.
        Assert.assertEquals("error", loadContentUrlToMakeCorsToContent("xhr", ""));
    }

    @Test
    @MediumTest
    @Feature({"CORS"})
    public void testContentUrlCanNotMakeFetchCorsRequestToContentUrl() throws Throwable {
        // The Fetch API does not support content:// scheme.
        Assert.assertEquals("error", loadContentUrlToMakeCorsToContent("fetch", "cors"));
    }

    @Test
    @MediumTest
    @Feature({"CORS"})
    public void testContentUrlCanNotMakeFetchSameOriginRequestToContentUrl() throws Throwable {
        // The Fetch API does not support content:// scheme.
        Assert.assertEquals("error", loadContentUrlToMakeCorsToContent("fetch", "same-origin"));
    }

    @Test
    @MediumTest
    @Feature({"CORS"})
    public void testContentUrlCanNotMakeFetchNoCorsRequestToContentUrl() throws Throwable {
        // The Fetch API does not support content:// scheme.
        Assert.assertEquals("error", loadContentUrlToMakeCorsToContent("fetch", "no-cors"));
    }

    @Test
    @MediumTest
    @Feature({"CORS"})
    public void testContentUrlToLoadWorkerFromContent() throws Throwable {
        final String resource = "content_url_load_content_worker.html";

        sActivityTestRule.loadUrl(createContentUrl(resource));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getTitleOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.not("running"));
                });

        // Make sure that content provider was asked to provide the content.
        ensureResourceRequestCountInContentProviderNotLessThan(resource, 1);

        Assert.assertEquals(
                "exception",
                ChromeTabUtils.getTitleOnUiThread(
                        sActivityTestRule.getActivity().getActivityTab()));
    }

    /** Test that a content URL is *ALLOWED* to access an image provided by a content URL. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlImageFromContentUrl() throws Throwable {
        verifyImageLoadRules(createContentUrl(SIMPLE_SRC), "success", 1);
    }

    /** Test that a HTTP URL is *NOT ALLOWED* to access an image provided by a content URL. */
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
        final String script =
                "var img = new Image();"
                        + "  img.onerror = function() { document.title = 'error' };"
                        + "  img.onabort = function() { document.title = 'error' };"
                        + "  img.onload = function() { document.title = 'success' };"
                        + "  img.src = '"
                        + createContentUrl(resource)
                        + "';"
                        + "  document.body.appendChild(img);";
        sActivityTestRule.loadUrl(url);
        sActivityTestRule.runJavaScriptCodeInCurrentTab(script);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getTitleOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is(expectedTitle));
                });
        ensureResourceRequestCountInContentProviderNotLessThan(resource, expectedLoadCount);
    }

    /** Test that a content URL is not allowed within a data URL. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromData() {
        final String target = SIMPLE_IMAGE;
        sActivityTestRule.loadUrl(
                UrlUtils.encodeHtmlDataUri("<img src=\"" + createContentUrl(target) + "\">"));
        ensureResourceRequestCountInContentProvider(target, 0);
    }

    /** Test that a content URL is not allowed within a local file. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromFile() throws IOException {
        final String target = SIMPLE_IMAGE;
        final File file = new File(Environment.getExternalStorageDirectory(), target + ".html");
        try {
            TestFileUtil.createNewHtmlFile(
                    file, target, "<img src=\"" + createContentUrl(target) + "\">");
            sActivityTestRule.loadUrl("file://" + file.getAbsolutePath());
            ensureResourceRequestCountInContentProvider(target, 0);
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    /** Test that the browser can be navigated to a file URL. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFileUrlNavigation() throws IOException {
        final File file =
                new File(Environment.getExternalStorageDirectory(), "url_navigation_test.html");

        try {
            TestFileUtil.createNewHtmlFile(file, "File", null);
            sActivityTestRule.loadUrl("file://" + file.getAbsolutePath());
            Assert.assertEquals(
                    "File",
                    ChromeTabUtils.getTitleOnUiThread(
                            sActivityTestRule.getActivity().getActivityTab()));
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     *
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = ApplicationProvider.getApplicationContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        Assert.assertEquals(expectedCount, actualCount);
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     *
     * @param resource Resource name
     * @param expectedMinimalCount Expected minimal resource requests count
     */
    private void ensureResourceRequestCountInContentProviderNotLessThan(
            String resource, int expectedMinimalCount) {
        Context context = ApplicationProvider.getApplicationContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        Assert.assertTrue(
                "Minimal expected: " + expectedMinimalCount + ", actual: " + actualCount,
                actualCount >= expectedMinimalCount);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }
}
