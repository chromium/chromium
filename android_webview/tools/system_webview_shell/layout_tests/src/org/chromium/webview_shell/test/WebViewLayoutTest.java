// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.os.Bundle;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.webview_shell.WebViewLayoutTestActivity;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests running end-to-end layout tests. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "https://crbug.com/1465624")
public class WebViewLayoutTest {
    private static final String TAG = "WebViewLayoutTest";

    private static final String EXTERNAL_PREFIX = UrlUtils.getIsolatedTestRoot() + "/";
    private static final String BASE_WEBVIEW_TEST_PATH =
            "android_webview/tools/system_webview_shell/test/data/";
    private static final String BASE_BLINK_TEST_PATH = "third_party/blink/web_tests/";
    private static final String PATH_WEBVIEW_PREFIX = EXTERNAL_PREFIX + BASE_WEBVIEW_TEST_PATH;
    private static final String PATH_BLINK_PREFIX = EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH;
    private static final String GLOBAL_LISTING_FILE =
            "webexposed/global-interface-listing-expected.txt";

    // Due to the specifics of the rebaselining algorithm in blink the files containing
    // stable interfaces can disappear and reappear later. To select the file to compare
    // against a fallback approach is used. The order in the List below is important due
    // to how blink performs baseline optimizations. For more details see
    // third_party/blink/tools/blinkpy/common/checkout/baseline_optimizer.py.
    private static final List<String> BLINK_STABLE_FALLBACKS =
            Arrays.asList(
                    EXTERNAL_PREFIX
                            + BASE_BLINK_TEST_PATH
                            + "virtual/stable/"
                            + GLOBAL_LISTING_FILE,
                    EXTERNAL_PREFIX
                            + BASE_BLINK_TEST_PATH
                            + "platform/linux/virtual/stable/"
                            + GLOBAL_LISTING_FILE,
                    EXTERNAL_PREFIX
                            + BASE_BLINK_TEST_PATH
                            + "platform/win/virtual/stable/"
                            + GLOBAL_LISTING_FILE,
                    EXTERNAL_PREFIX
                            + BASE_BLINK_TEST_PATH
                            + "platform/mac/virtual/stable/"
                            + GLOBAL_LISTING_FILE);

    private static final long TIMEOUT_SECONDS = 20;

    private static final String MODE_REBASELINE = "rebaseline";
    private static final String NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH =
            "//android_webview/tools/system_webview_shell/test/data/"
                    + "webexposed/not-webview-exposed.txt";

    private WebViewLayoutTestActivity mTestActivity;
    private boolean mRebaseLine;

    @Rule
    public BaseActivityTestRule<WebViewLayoutTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(WebViewLayoutTestActivity.class);

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mTestActivity = mActivityTestRule.getActivity();
        try {
            Bundle arguments = InstrumentationRegistry.getArguments();
            String modeArgument = arguments.getString("mode");
            mRebaseLine = MODE_REBASELINE.equals(modeArgument);
        } catch (IllegalStateException exception) {
            Log.w(TAG, "Got no instrumentation arguments", exception);
            mRebaseLine = false;
        }
    }

    @After
    public void tearDown() {
        mTestActivity.finish();
    }

    private boolean isRebaseline() {
        return mRebaseLine;
    }

    @Test
    @MediumTest
    public void testSimple() throws Exception {
        runWebViewLayoutTest(
                "experimental/basic-logging.html", "experimental/basic-logging-expected.txt");
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
        loadUrlWebViewAsync(
                "file://" + PATH_BLINK_PREFIX + "webexposed/global-interface-listing.html",
                mTestActivity);

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
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Process web test results.
        String result = mTestActivity.getTestResult();

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

    @Test
    @MediumTest
    public void testWebViewExcludedInterfaces() throws Exception {
        // Begin by running the web test.
        loadUrlWebViewAsync(
                "file://" + PATH_BLINK_PREFIX + "webexposed/global-interface-listing.html",
                mTestActivity);

        // Process all expectations files.
        String webviewExcluded =
                readFile(PATH_WEBVIEW_PREFIX + "webexposed/not-webview-exposed.txt");

        HashMap<String, HashSet<String>> webviewExcludedInterfacesMap =
                buildHashMap(webviewExcluded);

        // Wait for web test to finish running. Note we should wait for the web test to finish
        // running after processing all expectations files. All the expectations files have
        // a combined total of 300 lines and combined size of 12 KB. It is better to process
        // the expectations files in parallel with the web test run.
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Process web test results.
        String result = mTestActivity.getTestResult();
        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);

        HashSet<String> interfacesNotExposedInWebview = new HashSet<>();
        HashMap<String, HashSet<String>> propertiesNotExposedInWebview = new HashMap<>();

        // Check that each excluded interface and its properties are
        // not present in webviewInterfacesMap.
        for (Entry<String, HashSet<String>> entry : webviewExcludedInterfacesMap.entrySet()) {
            String interfaceInExcluded = entry.getKey();
            HashSet<String> excludedInterfaceProperties = entry.getValue();
            if (excludedInterfaceProperties.isEmpty()
                    && webviewInterfacesMap.containsKey(interfaceInExcluded)) {
                // An interface with an empty property list in not-webview-exposed.txt
                // should not be in the global-interface-listing.html results for WebView.
                interfacesNotExposedInWebview.add(interfaceInExcluded);
            }

            // global-interface-listing.html and not-webview-exposed.txt are mutually
            // exclusive.
            HashSet<String> actualInterfaceProperties =
                    Objects.requireNonNull(
                            webviewInterfacesMap.getOrDefault(
                                    interfaceInExcluded, new HashSet<>()));
            for (String excludedProperty : excludedInterfaceProperties) {
                if (actualInterfaceProperties.contains(excludedProperty)) {
                    propertiesNotExposedInWebview
                            .computeIfAbsent(interfaceInExcluded, k -> new HashSet<>())
                            .add(excludedProperty);
                }
            }
        }

        StringBuilder errorMessage = new StringBuilder();
        if (!interfacesNotExposedInWebview.isEmpty()) {
            errorMessage.append(
                    String.format(
                            """

                            The Blink interfaces below are exposed in WebView. Remove them from
                            %s
                             to resolve this error.
                            """,
                            NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            for (String illegallyExposedInterface : interfacesNotExposedInWebview) {
                errorMessage.append("\t- ").append(illegallyExposedInterface).append("\n");
            }
        }
        String errorTemplate =
                """

                %d of the properties of the Blink interface "%s"
                are exposed in WebView. Remove them from the list of properties excluded for the
                "%s" interface in
                %s
                to resolve this error
                """;
        for (Entry<String, HashSet<String>> entry : propertiesNotExposedInWebview.entrySet()) {
            String webviewInterface = entry.getKey();
            HashSet<String> propertiesNotExposed = entry.getValue();
            errorMessage.append(
                    String.format(
                            Locale.ROOT,
                            errorTemplate,
                            propertiesNotExposed.size(),
                            webviewInterface,
                            webviewInterface,
                            NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            for (String propertyNotExposed : propertiesNotExposed) {
                errorMessage.append("\t- ").append(propertyNotExposed).append("\n");
            }
        }
        if (errorMessage.length() > 0) {
            Assert.fail(errorMessage.toString());
        }
    }

    @Test
    @MediumTest
    @DisabledTest(message ="https://crbug.com/361258327")
    public void testWebViewIncludedStableInterfaces() throws Exception {
        // Begin by running the web test.
        loadUrlWebViewAsync(
                "file://" + PATH_BLINK_PREFIX + "webexposed/global-interface-listing.html",
                mTestActivity);

        // Process all expectations files.
        String blinkStableExpected = readFileWithFallbacks(BLINK_STABLE_FALLBACKS);
        String webviewExcluded =
                readFile(PATH_WEBVIEW_PREFIX + "webexposed/not-webview-exposed.txt");

        HashMap<String, HashSet<String>> webviewInterfacesMap;
        HashMap<String, HashSet<String>> blinkStableInterfacesMap =
                buildHashMap(blinkStableExpected);
        HashMap<String, HashSet<String>> webviewExcludedInterfacesMap =
                buildHashMap(webviewExcluded);

        // Wait for web test to finish running. Note we should wait for the web test to
        // finish running after processing all expectations files. All the expectations
        // files have a combined total of 9000 lines and combined size of 216 KB. It is
        // better to process the expectations files in parallel with the web test run.
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Process web test results.
        String result = mTestActivity.getTestResult();
        webviewInterfacesMap = buildHashMap(result);

        HashSet<String> missingInterfaces = new HashSet<>();
        HashMap<String, HashSet<String>> missingInterfaceProperties = new HashMap<>();

        // Check that each stable blink interface and its properties are present in webview
        // except the excluded interfaces/properties.
        for (HashMap.Entry<String, HashSet<String>> entry : blinkStableInterfacesMap.entrySet()) {
            String interfaceS = entry.getKey();
            HashSet<String> subsetExcluded = webviewExcludedInterfacesMap.get(interfaceS);

            if (subsetExcluded != null && subsetExcluded.isEmpty()) {
                // Interface is not exposed in WebView.
                continue;
            }

            HashSet<String> subsetBlink = entry.getValue();
            HashSet<String> subsetWebView = webviewInterfacesMap.get(interfaceS);

            if (subsetWebView == null) {
                // Interface is unexpectedly missing from WebView.
                missingInterfaces.add(interfaceS);
                continue;
            }

            for (String propertyBlink : subsetBlink) {
                if (subsetExcluded != null && subsetExcluded.contains(propertyBlink)) {
                    // At least one of the properties of this interface is excluded from WebView.
                    continue;
                }
                if (!subsetWebView.contains(propertyBlink)) {
                    // At least one of the properties of this interface is unexpectedly missing from
                    // WebView.
                    missingInterfaceProperties
                            .computeIfAbsent(interfaceS, k -> new HashSet<>())
                            .add(propertyBlink);
                }
            }
        }

        StringBuilder errorMessage = new StringBuilder();
        if (!missingInterfaces.isEmpty()) {
            errorMessage.append(
                    String.format(
                            """

                            WebView does not expose the Blink interfaces below. Add them to
                            %s
                            to resolve this error.
                            """,
                            NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            for (String missingInterface : missingInterfaces) {
                errorMessage.append("\t- ").append(missingInterface).append("\n");
            }
        }
        String errorTemplate =
                """

            At least one of the properties of the Blink interface "%s" is not exposed in WebView.
            Add them to the list of properties not exposed for the "%s" interface in
            %s
            to resolve this error
            """;
        for (Entry<String, HashSet<String>> entry : missingInterfaceProperties.entrySet()) {
            String blinkInterface = entry.getKey();
            HashSet<String> missingProperties = entry.getValue();
            errorMessage.append(
                    String.format(
                            errorTemplate,
                            blinkInterface,
                            blinkInterface,
                            NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            for (String missingProperty : missingProperties) {
                errorMessage.append("\t- ").append(missingProperty).append("\n");
            }
        }
        Assert.assertEquals(errorMessage.toString(), 0, errorMessage.length());
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccess() throws Exception {
        runWebViewLayoutTest(
                "blink-apis/webmidi/requestmidiaccess.html",
                "blink-apis/webmidi/requestmidiaccess-expected.txt");
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccessWithSysex() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest(
                "blink-apis/webmidi/requestmidiaccess-with-sysex.html",
                "blink-apis/webmidi/requestmidiaccess-with-sysex-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccessDenyPermission() throws Exception {
        runWebViewLayoutTest(
                "blink-apis/webmidi/requestmidiaccess-permission-denied.html",
                "blink-apis/webmidi/requestmidiaccess-permission-denied-expected.txt");
    }

    // Blink platform API tests

    @Test
    @MediumTest
    public void testGeolocationCallbacks() throws Exception {
        runWebViewLayoutTest(
                "blink-apis/geolocation/geolocation-permission-callbacks.html",
                "blink-apis/geolocation/geolocation-permission-callbacks-expected.txt");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add("use-fake-device-for-media-stream")
    public void testMediaStreamApiDenyPermission() throws Exception {
        runWebViewLayoutTest(
                "blink-apis/webrtc/mediastream-permission-denied-callbacks.html",
                "blink-apis/webrtc/mediastream-permission-denied-callbacks-expected.txt");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add("use-fake-device-for-media-stream")
    public void testMediaStreamApi() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest(
                "blink-apis/webrtc/mediastream-callbacks.html",
                "blink-apis/webrtc/mediastream-callbacks-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @Test
    @MediumTest
    public void testBatteryApi() throws Exception {
        runWebViewLayoutTest(
                "blink-apis/battery-status/battery-callback.html",
                "blink-apis/battery-status/battery-callback-expected.txt");
    }

    /*
    TODO(aluo): Investigate why this is failing on google devices too and not
    just aosp per crbug.com/607350
    */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/607350")
    public void testEMEPermission() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/eme/eme.html", "blink-apis/eme/eme-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    // test helper methods

    private void runWebViewLayoutTest(final String fileName, final String fileNameExpected)
            throws Exception {
        runTest(PATH_WEBVIEW_PREFIX + fileName, PATH_WEBVIEW_PREFIX + fileNameExpected, false);
    }

    private void runTest(final String fileName, final String fileNameExpected, boolean noFail)
            throws IOException, InterruptedException, TimeoutException {
        loadUrlWebViewAsync("file://" + fileName, mTestActivity);

        if (isRebaseline()) {
            // this is the rebaseline process
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            writeFile(fileNameExpected, result);
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
        } else {
            String expected = readFile(fileNameExpected);
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
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

    private void loadUrlWebViewAsync(
            final String fileUrl, final WebViewLayoutTestActivity activity) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> activity.loadUrl(fileUrl));
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
     * Reads the first available file in the 'fallback' list and returns the result. Throws
     * FileNotFoundException if non of the files exist.
     *
     * @noinspection SameParameterValue
     */
    private static String readFileWithFallbacks(List<String> fallbackFileNames) throws IOException {
        for (String fileName : fallbackFileNames) {
            File file = new File(fileName);
            if (file.exists()) {
                return readFile(fileName);
            }
        }

        throw new FileNotFoundException("None of the fallback files could be read");
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
