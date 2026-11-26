// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.util.Pair;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwDownloadHandler;
import org.chromium.android_webview.AwWebDownload;
import org.chromium.android_webview.AwWebDownloadResult;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Instrumentation tests for WebView-managed downloads (AwContents#setDownloadHandler). */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwDownloadHandlerTest extends AwParameterizedTest {
    private static final long TIMEOUT_SECONDS = 30;
    private static final String CONTENT = "Downloaded by WebView";

    @Rule public AwActivityTestRule mActivityTestRule;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwContents mAwContents;
    private File mDownloadDir;

    public AwDownloadHandlerTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        mDownloadDir =
                new File(ContextUtils.getApplicationContext().getFilesDir(), "download_handler");
        FileUtils.recursivelyDeleteFile(mDownloadDir, FileUtils.DELETE_ALL);
        Assert.assertTrue(mDownloadDir.mkdirs());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.clearDownloadHandler());
        FileUtils.recursivelyDeleteFile(mDownloadDir, FileUtils.DELETE_ALL);
    }

    private void setHandler(AwDownloadHandler handler) {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.setDownloadHandler(handler));
    }

    /** Loads {@code url} and returns the download that WebView passes to the handler. */
    private AwWebDownload startDownload(String url) throws Exception {
        CompletableFuture<AwWebDownload> future = new CompletableFuture<>();
        setHandler(
                download -> {
                    if (ThreadUtils.runningOnUiThread()) {
                        future.complete(download);
                    } else {
                        future.completeExceptionally(
                                new AssertionError("Handler not called on the UI thread"));
                    }
                });
        mActivityTestRule.loadUrlAsync(mAwContents, url);
        return future.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Calls saveTo() on the UI thread and returns a future with the {@link AwWebDownloadResult}.
     */
    private static CompletableFuture<Integer> saveTo(AwWebDownload download, Uri destination) {
        return saveTo(download, destination, new CompletableFuture<>());
    }

    /** Like {@link #saveTo(AwWebDownload, Uri)}, and completes {@code totalBytes} on success. */
    private static CompletableFuture<Integer> saveTo(
            AwWebDownload download, Uri destination, CompletableFuture<Long> totalBytes) {
        CompletableFuture<Integer> result = new CompletableFuture<>();
        AwWebDownload.SaveCallback callback =
                new AwWebDownload.SaveCallback() {
                    @Override
                    public void onSuccess(long bytes) {
                        totalBytes.complete(bytes);
                        complete(AwWebDownloadResult.SUCCESS);
                    }

                    @Override
                    public void onError(int errorCode) {
                        complete(errorCode);
                    }

                    private void complete(int value) {
                        if (ThreadUtils.runningOnUiThread()) {
                            result.complete(value);
                        } else {
                            result.completeExceptionally(
                                    new AssertionError("Callback not called on the UI thread"));
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> download.saveTo(destination, callback));
        return result;
    }

    private static int await(CompletableFuture<Integer> result) throws Exception {
        return result.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private static void cancel(AwWebDownload download) {
        ThreadUtils.runOnUiThreadBlocking(download::cancel);
    }

    private static List<Pair<String, String>> attachmentHeaders(String mimeType, String filename) {
        List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(Pair.create("Content-Type", mimeType));
        headers.add(
                Pair.create("Content-Disposition", "attachment; filename=\"" + filename + "\""));
        return headers;
    }

    /** Serves {@link #CONTENT} as a text attachment named {@code filename} and returns its URL. */
    private static String serveAttachment(TestWebServer server, String filename) {
        return server.setResponse(
                "/" + filename, CONTENT, attachmentHeaders("text/plain", filename));
    }

    /**
     * Serves a download that never ends on its own, and returns a future that completes once the
     * client closes the connection.
     */
    private static CompletableFuture<Void> serveEndlessDownload(WebServer server) {
        return serveEndlessDownload(server, "application/octet-stream", "endless.bin");
    }

    private static CompletableFuture<Void> serveEndlessDownload(
            WebServer server, String mimeType, String filename) {
        CompletableFuture<Void> disconnected = new CompletableFuture<>();
        server.setRequestHandler(
                (request, output) -> {
                    try {
                        output.write(
                                ("HTTP/1.1 200 OK\r\n"
                                                + "Content-Type: "
                                                + mimeType
                                                + "\r\n"
                                                + "Content-Disposition: attachment; filename=\""
                                                + filename
                                                + "\"\r\n"
                                                + "Connection: close\r\n\r\n")
                                        .getBytes(StandardCharsets.US_ASCII));
                        while (true) {
                            output.write(new byte[8192]);
                            output.flush();
                            Thread.sleep(10);
                        }
                    } catch (IOException e) {
                        disconnected.complete(null);
                    } catch (InterruptedException e) {
                        disconnected.completeExceptionally(e);
                    }
                });
        return disconnected;
    }

    /**
     * Loads {@code url} from an endless download handler and asserts that WebView cancels the
     * transfer without passing the download to the handler.
     */
    private void assertDownloadBlocked(String url, CompletableFuture<Void> disconnected)
            throws Exception {
        AtomicBoolean handlerCalled = new AtomicBoolean();
        setHandler(
                download -> {
                    handlerCalled.set(true);
                    download.cancel();
                });
        mActivityTestRule.loadUrlAsync(mAwContents, url);
        disconnected.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        ThreadUtils.runOnUiThreadBlocking(() -> Assert.assertFalse(handlerCalled.get()));
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testHandlerReplacesLegacyDownloadListener() throws Exception {
        OnDownloadStartHelper legacyHelper = mContentsClient.getOnDownloadStartHelper();
        int legacyCallCount = legacyHelper.getCallCount();
        try (TestWebServer server = TestWebServer.start()) {
            // With a handler, the download goes to the handler.
            AwWebDownload download = startDownload(serveAttachment(server, "handler.txt"));
            cancel(download);

            // Without a handler, the download goes to the legacy DownloadListener.
            ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.clearDownloadHandler());
            String legacyUrl = serveAttachment(server, "legacy.txt");
            mActivityTestRule.loadUrlAsync(mAwContents, legacyUrl);
            legacyHelper.waitForCallback(legacyCallCount);
            // Legacy callbacks are delivered in order, so a callback for the first download would
            // have arrived first.
            Assert.assertEquals(legacyUrl, legacyHelper.getUrl());
            Assert.assertEquals(legacyCallCount + 1, legacyHelper.getCallCount());
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testLegacyDownloadListenerReceivesApk() throws Exception {
        // Dangerous file types like APKs are only blocked for the download handler. Without a
        // handler, they still go to the legacy DownloadListener.
        OnDownloadStartHelper legacyHelper = mContentsClient.getOnDownloadStartHelper();
        int legacyCallCount = legacyHelper.getCallCount();
        try (TestWebServer server = TestWebServer.start()) {
            String mimeType = "application/vnd.android.package-archive";
            String url =
                    server.setResponse("/app.apk", CONTENT, attachmentHeaders(mimeType, "app.apk"));
            mActivityTestRule.loadUrlAsync(mAwContents, url);
            legacyHelper.waitForCallback(legacyCallCount);
            Assert.assertEquals(url, legacyHelper.getUrl());
            Assert.assertEquals(mimeType, legacyHelper.getMimeType());
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testSaveToFile() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            AwWebDownload download = startDownload(serveAttachment(server, "file.txt"));

            File destination = new File(mDownloadDir, "file.txt");
            CompletableFuture<Long> totalBytes = new CompletableFuture<>();
            Assert.assertEquals(
                    AwWebDownloadResult.SUCCESS,
                    await(saveTo(download, Uri.fromFile(destination), totalBytes)));
            Assert.assertEquals(CONTENT.length(), destination.length());
            Assert.assertEquals(CONTENT.length(), (long) totalBytes.get());
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testDownloadProperties() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String url = serveAttachment(server, "report.txt");
            AwWebDownload download = startDownload(server.setRedirect("/redirect", url));

            // The URL after redirects.
            Assert.assertEquals(url, download.getUrl());
            Assert.assertEquals("text/plain", download.getMimeType());
            Assert.assertEquals(CONTENT.length(), download.getContentLength());
            Assert.assertEquals("report.txt", download.getSuggestedFilename());
            cancel(download);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testSaveToMediaStore() throws Exception {
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        ContentValues values = new ContentValues();
        values.put(
                MediaStore.MediaColumns.DISPLAY_NAME,
                "aw-download-handler-test-" + System.nanoTime() + ".txt");
        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
        values.put(MediaStore.MediaColumns.IS_PENDING, 1);
        Uri item = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        Assert.assertNotNull(item);
        try (TestWebServer server = TestWebServer.start()) {
            AwWebDownload download = startDownload(serveAttachment(server, "media.txt"));

            CompletableFuture<Long> totalBytes = new CompletableFuture<>();
            Assert.assertEquals(
                    AwWebDownloadResult.SUCCESS, await(saveTo(download, item, totalBytes)));
            Assert.assertEquals(CONTENT.length(), (long) totalBytes.get());
            try (ParcelFileDescriptor file = resolver.openFileDescriptor(item, "r")) {
                Assert.assertEquals(CONTENT.length(), file.getStatSize());
            }
            // WebView publishes the item. An item left pending stays hidden and expires.
            try (Cursor cursor =
                    resolver.query(
                            item,
                            new String[] {MediaStore.MediaColumns.IS_PENDING},
                            null,
                            null,
                            null)) {
                Assert.assertTrue(cursor.moveToFirst());
                Assert.assertEquals(0, cursor.getInt(0));
            }
        } finally {
            resolver.delete(item, null, null);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testCancelBeforeSaveTo() throws Exception {
        try (WebServer server = new WebServer(0, false)) {
            CompletableFuture<Void> disconnected = serveEndlessDownload(server);
            AwWebDownload download = startDownload(server.getResponseUrl("/endless.bin"));

            cancel(download);
            // WebView stops the transfer.
            disconnected.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testCancelAfterSaveTo() throws Exception {
        try (WebServer server = new WebServer(0, false)) {
            serveEndlessDownload(server);
            AwWebDownload download = startDownload(server.getResponseUrl("/endless.bin"));

            CompletableFuture<Integer> result =
                    saveTo(download, Uri.fromFile(new File(mDownloadDir, "endless.bin")));
            cancel(download);
            Assert.assertEquals(AwWebDownloadResult.ERROR_CANCELLED, await(result));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testUnwritableDestinationReportsFileError() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            AwWebDownload download = startDownload(serveAttachment(server, "unwritable.txt"));

            // The destination cannot be created because its parent is a file.
            File parent = new File(mDownloadDir, "not_a_directory");
            Assert.assertTrue(parent.createNewFile());
            File destination = new File(parent, "unwritable.txt");
            Assert.assertEquals(
                    AwWebDownloadResult.ERROR_FILE,
                    await(saveTo(download, Uri.fromFile(destination))));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testTruncatedDownloadReportsNetworkError() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            // The response declares more bytes than it sends.
            List<Pair<String, String>> headers =
                    attachmentHeaders("application/octet-stream", "truncated.bin");
            headers.add(Pair.create("Content-Length", "100000"));
            AwWebDownload download =
                    startDownload(server.setResponse("/truncated.bin", new byte[4096], headers));

            File destination = new File(mDownloadDir, "truncated.bin");
            Assert.assertEquals(
                    AwWebDownloadResult.ERROR_NETWORK,
                    await(saveTo(download, Uri.fromFile(destination))));
        }
    }

    private static List<Pair<String, String>> contentTypeHeaders(String mimeType) {
        List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(Pair.create("Content-Type", mimeType));
        return headers;
    }

    private static String serveInlinePdf(TestWebServer server) {
        return server.setResponse("/inline.pdf", CONTENT, contentTypeHeaders("application/pdf"));
    }

    private static String servePage(TestWebServer server, String path, String html) {
        return server.setResponse(path, html, contentTypeHeaders("text/html"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testIsExplicitDownload() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            AwWebDownload attachment = startDownload(serveAttachment(server, "file.txt"));
            Assert.assertTrue(attachment.isExplicitDownload());
            cancel(attachment);

            AwWebDownload inlinePdf = startDownload(serveInlinePdf(server));
            Assert.assertFalse(inlinePdf.isExplicitDownload());
            cancel(inlinePdf);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testIsForMainFrame() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String pdfUrl = serveInlinePdf(server);
            AwWebDownload mainFrame = startDownload(pdfUrl);
            Assert.assertTrue(mainFrame.isForMainFrame());
            cancel(mainFrame);

            String pageUrl =
                    servePage(server, "/iframe.html", "<iframe src='" + pdfUrl + "'></iframe>");
            AwWebDownload iframe = startDownload(pageUrl);
            Assert.assertFalse(iframe.isForMainFrame());
            cancel(iframe);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testHasGesture() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String fileUrl = serveAttachment(server, "file.txt");
            String pageUrl =
                    servePage(server, "/link.html", "<a id='link' href='" + fileUrl + "'>link</a>");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            CompletableFuture<AwWebDownload> future = new CompletableFuture<>();
            setHandler(future::complete);
            JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "link");
            AwWebDownload tapped = future.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertTrue(tapped.hasGesture());
            cancel(tapped);

            AwWebDownload loaded = startDownload(fileUrl);
            Assert.assertFalse(loaded.hasGesture());
            cancel(loaded);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBlocksApkByExtension() throws Exception {
        try (WebServer server = new WebServer(0, false)) {
            CompletableFuture<Void> disconnected =
                    serveEndlessDownload(server, "application/octet-stream", "app.apk");
            assertDownloadBlocked(server.getResponseUrl("/app.apk"), disconnected);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBlocksApkByMimeType() throws Exception {
        try (WebServer server = new WebServer(0, false)) {
            CompletableFuture<Void> disconnected =
                    serveEndlessDownload(
                            server, "application/vnd.android.package-archive", "app.bin");
            assertDownloadBlocked(server.getResponseUrl("/app.bin"), disconnected);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBlocksDangerousExtensionWithTrailingDot() throws Exception {
        try (WebServer server = new WebServer(0, false)) {
            CompletableFuture<Void> disconnected =
                    serveEndlessDownload(server, "application/octet-stream", "app.APK.");
            assertDownloadBlocked(server.getResponseUrl("/download"), disconnected);
        }
    }
}
