// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import junit.framework.ComparisonFailure;

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
import org.chromium.base.test.util.UrlUtils;
import org.chromium.webview_shell.WebViewLayoutTestActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests running end-to-end layout tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
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
    // stable interfaces can dissapear and reappear later. To select the file to compare
    // against a fallback approach is used. The order in the List below is important due
    // to how blink performs baseline optimizations. For more details see
    // third_party/blink/tools/blinkpy/common/checkout/baseline_optimizer.py.
    private static final List<String> BLINK_STABLE_FALLBACKS = Arrays.asList(
            EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH + "virtual/stable/" + GLOBAL_LISTING_FILE,
            EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH + "platform/linux/virtual/stable/"
                    + GLOBAL_LISTING_FILE,
            EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH + "platform/win/virtual/stable/"
                    + GLOBAL_LISTING_FILE,
            EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH + "platform/mac/virtual/stable/"
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
        Bundle arguments = InstrumentationRegistry.getArguments();
        if (arguments != null) {
            String modeArgument = arguments.getString("mode");
            mRebaseLine = modeArgument != null ? modeArgument.equals(MODE_REBASELINE) : false;
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
        runWebViewLayoutTest("experimental/basic-logging.html",
                             "experimental/basic-logging-expected.txt");
    }

    // This is a non-failing test because it tends to require frequent rebaselines.
    @Test
    @MediumTest
    public void testGlobalInterfaceNoFail() throws Exception {
        runBlinkLayoutTest("webexposed/global-interface-listing.html",
                           "webexposed/global-interface-listing-expected.txt", true);
    }

    // This is a non-failing test to avoid 'blind' rebaselines by the sheriff
    // (see crbug.com/564765).
    @Test
    @MediumTest
    public void testNoUnexpectedInterfaces() throws Exception {
        // Begin by running the web test.
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);

        // Process all expectations files.
        String webviewExpected = readFile(PATH_WEBVIEW_PREFIX
                + "webexposed/global-interface-listing-expected.txt");
        HashMap<String, HashSet<String>> webviewExpectedInterfacesMap =
                buildHashMap(webviewExpected);

        // Wait for web test to finish running. Note we should wait for the web test to
        // finish running after processing the expectations file. The expectations file
        // has 8600 lines and a size of 212 KB. It is better to process the expectations
        // file in parallel with the web test run.
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Process web test results.
        String result = mTestActivity.getTestResult();
        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);

        StringBuilder newInterfaces = new StringBuilder();

        // Check that each current webview interface is one of webview expected interfaces.
        for (String interfaceS : webviewInterfacesMap.keySet()) {
            if (webviewExpectedInterfacesMap.get(interfaceS) == null) {
                newInterfaces.append(interfaceS + "\n");
            }
        }

        if (newInterfaces.length() > 0) {
            Log.w(TAG, "Unexpected WebView interfaces found: " + newInterfaces.toString());
        }
    }

    @Test
    @MediumTest
    public void testWebViewExcludedInterfaces() throws Exception {
        // Begin by running the web test.
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);

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
        webviewExcludedInterfacesMap.forEach((interfaceInExcluded, excludedInterfaceProperties) -> {
            if (excludedInterfaceProperties.isEmpty()
                    && webviewInterfacesMap.containsKey(interfaceInExcluded)) {
                // An interface with an empty property list in not-webview-exposed.txt
                // should not be in the global-interface-listing.html results for WebView.
                interfacesNotExposedInWebview.add(interfaceInExcluded);
            }

            // global-interface-listing.html and not-webview-exposed.txt are mutually exclusive.
            excludedInterfaceProperties.forEach((excludedProperty) -> {
                if (webviewInterfacesMap.getOrDefault(interfaceInExcluded, new HashSet<>())
                                .contains(excludedProperty)) {
                    propertiesNotExposedInWebview.putIfAbsent(interfaceInExcluded, new HashSet<>());
                    propertiesNotExposedInWebview.get(interfaceInExcluded).add(excludedProperty);
                }
            });
        });

        StringBuilder errorMessage = new StringBuilder();
        if (!interfacesNotExposedInWebview.isEmpty()) {
            errorMessage.append(
                    String.format("\nThe Blink interfaces below are exposed in WebView. "
                                    + "Remove them from\n%s\n to resolve this error.\n",
                            NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            interfacesNotExposedInWebview.forEach(illegallyExposedInterface -> {
                errorMessage.append("\t- " + illegallyExposedInterface + "\n");
            });
        }
        propertiesNotExposedInWebview.forEach((webviewInterface, propertiesNotExposed) -> {
            errorMessage.append(String.format("\n%d of the properties of the Blink interface "
                            + "\"%s\"\nare exposed in WebView. Remove them from the "
                            + "list of properties excluded for the \n\"%s\" interface in \n%s\n "
                            + "to resolve this error\n",
                    propertiesNotExposed.size(), webviewInterface, webviewInterface,
                    NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            propertiesNotExposed.forEach(propertyNotExposed -> {
                errorMessage.append("\t- " + propertyNotExposed + "\n");
            });
        });
        if (errorMessage.length() > 0) {
            Assert.fail(errorMessage.toString());
        }
    }

    @Test
    @MediumTest
    public void testWebViewIncludedStableInterfaces() throws Exception {
        // Begin by running the web test.
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);

        // Process all expectations files.
        String blinkStableExpected = readFileWithFallbacks(BLINK_STABLE_FALLBACKS);
        String webviewExcluded =
                readFile(PATH_WEBVIEW_PREFIX + "webexposed/not-webview-exposed.txt");

        HashMap<String, HashSet<String>> webviewInterfacesMap;
        HashMap<String, HashSet<String>> blinkStableInterfacesMap =
                buildHashMap(blinkStableExpected);
        HashMap<String, HashSet<String>> webviewExcludedInterfacesMap =
                buildHashMap(webviewExcluded);
        StringBuilder missing = new StringBuilder();

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
                    missingInterfaceProperties.putIfAbsent(interfaceS, new HashSet<>());
                    missingInterfaceProperties.get(interfaceS).add(propertyBlink);
                }
            }
        }

        StringBuilder errorMessage = new StringBuilder();
        if (!missingInterfaces.isEmpty()) {
            errorMessage.append(String.format("\nWebView does not expose the "
                            + "Blink interfaces below. Add them to\n%s\nto resolve this error.\n",
                    NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            missingInterfaces.forEach(
                    (missingInterface) -> errorMessage.append("\t- " + missingInterface + "\n"));
        }
        missingInterfaceProperties.forEach((blinkInterface, missingProperties) -> {
            errorMessage.append(String.format(
                    "\nAt least one of the properties of the Blink interface \"%s\" "
                            + "is not exposed in WebView.\nAdd them to the list of properties "
                            + "not exposed for the \"%s\" interface in\n%s\nto resolve "
                            + "this error\n",
                    blinkInterface, blinkInterface, NOT_WEBVIEW_EXPOSED_CHROMIUM_PATH));
            missingProperties.forEach(
                    (missingProperty) -> errorMessage.append("\t- " + missingProperty + "\n"));
        });
        Assert.assertTrue(errorMessage.toString(), errorMessage.length() == 0);
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccess() throws Exception {
        runWebViewLayoutTest("blink-apis/webmidi/requestmidiaccess.html",
                "blink-apis/webmidi/requestmidiaccess-expected.txt");
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccessWithSysex() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/webmidi/requestmidiaccess-with-sysex.html",
                "blink-apis/webmidi/requestmidiaccess-with-sysex-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccessDenyPermission() throws Exception {
        runWebViewLayoutTest("blink-apis/webmidi/requestmidiaccess-permission-denied.html",
                "blink-apis/webmidi/requestmidiaccess-permission-denied-expected.txt");
    }

    // Blink platform API tests

    @Test
    @MediumTest
    public void testGeolocationCallbacks() throws Exception {
        runWebViewLayoutTest("blink-apis/geolocation/geolocation-permission-callbacks.html",
                "blink-apis/geolocation/geolocation-permission-callbacks-expected.txt");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add("use-fake-device-for-media-stream")
    public void testMediaStreamApiDenyPermission() throws Exception {
        runWebViewLayoutTest("blink-apis/webrtc/mediastream-permission-denied-callbacks.html",
                "blink-apis/webrtc/mediastream-permission-denied-callbacks-expected.txt");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add("use-fake-device-for-media-stream")
    public void testMediaStreamApi() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/webrtc/mediastream-callbacks.html",
                "blink-apis/webrtc/mediastream-callbacks-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @Test
    @MediumTest
    public void testBatteryApi() throws Exception {
        runWebViewLayoutTest("blink-apis/battery-status/battery-callback.html",
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

    private void runBlinkLayoutTest(
            final String fileName, final String fileNameExpected, boolean noFail) throws Exception {
        runTest(PATH_BLINK_PREFIX + fileName, PATH_WEBVIEW_PREFIX + fileNameExpected, noFail);
    }

    private void runTest(final String fileName, final String fileNameExpected, boolean noFail)
            throws FileNotFoundException, IOException, InterruptedException, TimeoutException {
        loadUrlWebViewAsync("file://" + fileName, mTestActivity);

        if (isRebaseline()) {
            // this is the rebaseline process
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            writeFile(fileNameExpected, result, true);
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
        } else {
            String expected = readFile(fileNameExpected);
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            if (noFail && !expected.equals(result)) {
                ComparisonFailure cf = new ComparisonFailure("Unexpected result", expected, result);
                Log.e(TAG, cf.toString());
            } else {
                Assert.assertEquals(expected, result);
            }
        }
    }

    private void loadUrlWebViewAsync(final String fileUrl,
            final WebViewLayoutTestActivity activity) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                activity.loadUrl(fileUrl);
            }
        });
    }

    /**
     * Reads a file and returns it's contents as string.
     */
    private static String readFile(String fileName) throws IOException {
        FileInputStream inputStream = new FileInputStream(new File(fileName));
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
            try {
                StringBuilder contents = new StringBuilder();
                String line;

                while ((line = reader.readLine()) != null) {
                    contents.append(line);
                    contents.append("\n");
                }
                return contents.toString();
            } finally {
                reader.close();
            }
        } finally {
            inputStream.close();
        }
    }

    /**
     * Reads the first available file in the 'fallback' list and returns the result.
     * Throws FileNotFoundException if non of the files exist.
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
     * Writes a file with the given fileName and contents. If overwrite is true overwrites any
     * exisiting file with the same file name. If the file does not exist any intermediate
     * required directories are created.
     */
    private static void writeFile(final String fileName, final String contents, boolean overwrite)
            throws FileNotFoundException, IOException {
        File fileOut = new File(fileName);

        if (fileOut.exists() && !overwrite) {
            return;
        }

        String absolutePath = fileOut.getAbsolutePath();
        File filePath = new File(absolutePath.substring(0, absolutePath.lastIndexOf("/")));

        if (!filePath.exists()) {
            if (!filePath.mkdirs()) {
                throw new IOException("failed to create directories: " + filePath);
            }
        }

        FileOutputStream outputStream = new FileOutputStream(fileOut);
        try {
            outputStream.write(contents.getBytes());
        } finally {
            outputStream.close();
        }
    }

    private HashMap<String, HashSet<String>> buildHashMap(String contents) {
        HashMap<String, HashSet<String>> interfaces = new HashMap<>();

        return buildHashMap(contents, interfaces);
    }

    private HashMap<String, HashSet<String>> buildHashMap(
            String contents, HashMap<String, HashSet<String>> interfaces) {
        String[] lineByLine = contents.split("\\n");

        HashSet<String> subset = null;
        for (String line : lineByLine) {
            String s = trimAndRemoveComments(line);
            if (isInterfaceOrGlobalObject(s)) {
                subset = interfaces.get(s);
                if (subset == null) {
                    subset = new HashSet<>();
                    interfaces.put(s, subset);
                }
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
        return s.startsWith("getter") || s.startsWith("setter")
                || s.startsWith("method") || s.startsWith("attribute");
    }

}
