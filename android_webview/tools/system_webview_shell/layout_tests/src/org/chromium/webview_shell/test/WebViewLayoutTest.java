// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.webview_shell.WebViewLayoutTestActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
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
    private static final String PATH_WEBVIEW_PREFIX = EXTERNAL_PREFIX + BASE_WEBVIEW_TEST_PATH;

    private static final long TIMEOUT_SECONDS = 20;

    // LINT.IfChange
    private static final String EXTRA_REBASELINE =
            "org.chromium.android_webview.test.RebaselineMode";
    private static final String MODE_REBASELINE = "rebaseline";
    // LINT.ThenChange(//android_webview/javatests/src/org/chromium/android_webview/test/WebExposedTest.java)

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
            String modeArgument = arguments.getString(EXTRA_REBASELINE);
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

    @Test
    @MediumTest
    public void testRequestMIDIAccess() throws Exception {
        if (!hasSystemFeatureMidi()) {
            return;
        }
        runWebViewLayoutTest(
                "blink-apis/webmidi/requestmidiaccess.html",
                "blink-apis/webmidi/requestmidiaccess-expected.txt");
    }

    @Test
    @MediumTest
    public void testRequestMIDIAccessWithSysex() throws Exception {
        if (!hasSystemFeatureMidi()) {
            return;
        }
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

    /**
     * Checks if the device has the MIDI system feature.
     */
    private static boolean hasSystemFeatureMidi() {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_MIDI);
    }
}
