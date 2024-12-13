// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;
import android.webkit.JavascriptInterface;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UrlUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** WebExposed tests implemented as an instrumentation test instead of a layout test. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "crbug.com/381090604 - undergoing refactor")
public class WebExposedTest extends AwParameterizedTest {
    private static final String TAG = "WebExposedTest";

    private static final String EXTERNAL_PREFIX = UrlUtils.getIsolatedTestRoot() + "/";
    private static final String BASE_WEBVIEW_TEST_PATH = "android_webview/test/data/web_tests/";
    private static final String BASE_BLINK_TEST_PATH = "third_party/blink/web_tests/";
    private static final String PATH_WEBVIEW_PREFIX = EXTERNAL_PREFIX + BASE_WEBVIEW_TEST_PATH;
    private static final String PATH_BLINK_PREFIX = EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH;
    private static final String GLOBAL_LISTING_FILE =
            "webexposed/global-interface-listing-expected.txt";

    private static final String TEST_FINISHED_SENTINEL = "TEST FINISHED";
    private static final long TIMEOUT_SECONDS = 20;

    // LINT.IfChange
    private static final String EXTRA_REBASELINE =
            "org.chromium.android_webview.test.RebaselineMode";
    private static final String MODE_REBASELINE = "rebaseline";
    // LINT.ThenChange(//build/android/pylib/local/device/local_device_instrumentation_test_run.py)

    @Rule public AwActivityTestRule mRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private final boolean mRebaseline;

    private final StringBuilder mConsoleLog = new StringBuilder();
    private final Object mLock = new Object();
    private boolean mFinished;

    public WebExposedTest(AwSettingsMutation param) {
        mRule = new AwActivityTestRule(param.getMutation());
        boolean rebaseline;
        try {
            Bundle arguments = InstrumentationRegistry.getArguments();
            String modeArgument = arguments.getString(EXTRA_REBASELINE);
            rebaseline = MODE_REBASELINE.equals(modeArgument);
        } catch (IllegalStateException exception) {
            rebaseline = false;
        }
        mRebaseline = rebaseline;
    }

    private boolean isRebaseline() {
        return mRebaseline;
    }

    @Before
    public void setUp() throws Throwable {
        mContentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onReceivedError(
                            AwWebResourceRequest request, AwWebResourceError error) {
                        Assert.fail(
                                "onReceivedError: "
                                        + error.description
                                        + ", "
                                        + request.url
                                        + "\n");
                    }

                    @Override
                    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
                        if (consoleMessage.messageLevel() == AwConsoleMessage.MESSAGE_LEVEL_LOG) {
                            mConsoleLog.append(consoleMessage.message() + "\n");
                            if (consoleMessage.message().equals(TEST_FINISHED_SENTINEL)) {
                                finishTest();
                            }
                        } else {
                            Assert.fail(
                                    "Unexpected non-log level console message: "
                                            + consoleMessage.message());
                        }
                        return true;
                    }
                };

        AwTestContainerView mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwSettings settings = mAwContents.getSettings();
                    settings.setAllowFileAccess(true);
                    settings.setAllowFileAccessFromFileUrls(true);
                    settings.setJavaScriptEnabled(true);

                    // Exposes window.openDatabase
                    settings.setDatabaseEnabled(true);

                    class SynchronousConsole {
                        @JavascriptInterface
                        public void log(String message) {
                            mConsoleLog.append(message + "\n");
                        }
                    }

                    mAwContents.addJavascriptInterface(new SynchronousConsole(), "awConsole");
                });
    }

    // This is a non-failing test because it tends to require frequent rebaselines.
    @Test
    @MediumTest
    public void testGlobalInterfaceNoFail() throws Exception {
        runTest(
                PATH_BLINK_PREFIX + "webexposed/global-interface-listing.html",
                PATH_WEBVIEW_PREFIX + "webexposed/global-interface-listing-expected.txt",
                true);
    }

    // This is a non-failing test to avoid rebaselines by the sheriff
    // (see crbug.com/564765).
    @Test
    @MediumTest
    public void testNoUnexpectedInterfaces() throws Exception {
        // Begin by running the web test.
        mRule.loadUrlAsync(
                mAwContents,
                "file://" + PATH_BLINK_PREFIX + "webexposed/global-interface-listing.html");

        // Process all expectations files.
        String fileNameExpected =
                PATH_WEBVIEW_PREFIX + "webexposed/global-interface-listing-expected.txt";
        String webviewExpected = readFile(fileNameExpected);
        HashMap<String, HashSet<String>> webviewExpectedInterfacesMap =
                buildHashMap(webviewExpected);

        // Wait for web test to finish running. Note we should wait for the web test to
        // finish running after processing the expectations file. The expectations file
        // has 8600 lines and a size of 212 KB. It is better to process the expectations
        // file in parallel with the web test run.
        waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Process web test results.
        String result = getTestResult();

        if (isRebaseline()) {
            writeFile(fileNameExpected, result);
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
            return;
        }

        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);

        StringBuilder newInterfaces = new StringBuilder();

        // Check that each current webview interface is one of webview expected interfaces.
        for (String interfaceS : webviewInterfacesMap.keySet()) {
            if (webviewExpectedInterfacesMap.get(interfaceS) == null) {
                newInterfaces.append(interfaceS).append("\n");
            }
        }

        if (newInterfaces.length() > 0) {
            Log.w(TAG, "Unexpected WebView interfaces found: " + newInterfaces);
        }
    }

    private void runTest(final String fileName, final String fileNameExpected, boolean noFail)
            throws Exception {
        mRule.loadUrlAsync(mAwContents, "file://" + fileName);

        if (isRebaseline()) {
            // this is the rebaseline process
            waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = getTestResult();
            writeFile(fileNameExpected, result);
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
        } else {
            String expected = readFile(fileNameExpected);
            waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = getTestResult();
            try {
                Assert.assertEquals(expected, result);
            } catch (AssertionError exception) {
                if (noFail) {
                    Log.e(TAG, "%s", exception.toString());
                } else {
                    throw exception;
                }
            }
        }
    }

    /** Reads a file and returns it's contents as string. */
    private static String readFile(String fileName) throws IOException {
        StringBuilder sb = new StringBuilder();
        for (String line : Files.readAllLines(Paths.get(fileName), StandardCharsets.UTF_8)) {
            // Test output has newlines at end of every line. This ensures the loaded expectation
            // file also has these newlines.
            sb.append(line).append("\n");
        }
        return sb.toString();
    }

    /**
     * Writes a file with the given fileName and contents. If the file does not exist any
     * intermediate required directories are created.
     */
    private static void writeFile(final String fileName, final String contents) throws IOException {
        File fileOut = new File(fileName);

        String absolutePath = fileOut.getAbsolutePath();
        File filePath = new File(absolutePath.substring(0, absolutePath.lastIndexOf("/")));

        if (!filePath.exists()) {
            if (!filePath.mkdirs()) {
                throw new IOException("failed to create directories: " + filePath);
            }
        }
        try (FileOutputStream outputStream = new FileOutputStream(fileOut)) {
            outputStream.write(contents.getBytes(StandardCharsets.UTF_8));
        }
    }

    public String getTestResult() {
        return mConsoleLog.toString();
    }

    public void waitForFinish(long timeout, TimeUnit unit)
            throws InterruptedException, TimeoutException {
        synchronized (mLock) {
            long deadline = System.currentTimeMillis() + unit.toMillis(timeout);
            while (!mFinished && System.currentTimeMillis() < deadline) {
                mLock.wait(deadline - System.currentTimeMillis());
            }
            if (!mFinished) {
                throw new TimeoutException("timeout");
            }
        }
    }

    private void finishTest() {
        mFinished = true;
        synchronized (mLock) {
            mLock.notifyAll();
        }
    }

    private HashMap<String, HashSet<String>> buildHashMap(String contents) {
        HashMap<String, HashSet<String>> interfaces = new HashMap<>();
        String[] lineByLine = contents.split("\\n");

        HashSet<String> subset = null;
        for (String line : lineByLine) {
            String s = trimAndRemoveComments(line);
            if (isInterfaceOrGlobalObject(s)) {
                subset = interfaces.computeIfAbsent(s, k -> new HashSet<>());
            } else if (isInterfaceProperty(s) && subset != null) {
                subset.add(s);
            }
        }
        return interfaces;
    }

    private String trimAndRemoveComments(String line) {
        String s = line.trim();
        int commentIndex = s.indexOf("#"); // remove any potential comments
        return (commentIndex >= 0) ? s.substring(0, commentIndex).trim() : s;
    }

    private boolean isInterfaceOrGlobalObject(String s) {
        return s.startsWith("interface") || s.startsWith("[GLOBAL OBJECT]");
    }

    private boolean isInterfaceProperty(String s) {
        return s.startsWith("getter")
                || s.startsWith("setter")
                || s.startsWith("method")
                || s.startsWith("attribute");
    }
}
