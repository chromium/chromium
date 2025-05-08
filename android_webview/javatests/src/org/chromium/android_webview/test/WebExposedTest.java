// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;
import android.webkit.JavascriptInterface;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.common.util.concurrent.SettableFuture;

import difflib.DiffUtils;
import difflib.Patch;

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
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.List;

/** WebExposed tests implemented as an instrumentation test instead of a layout test. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class WebExposedTest extends AwParameterizedTest {
    private static final String TAG = "WebExposedTest";

    private static final String EXTERNAL_PREFIX = UrlUtils.getIsolatedTestRoot() + "/";
    private static final String BASE_WEBVIEW_TEST_PATH = "android_webview/test/data/web_tests/";
    private static final String BASE_BLINK_TEST_PATH = "third_party/blink/web_tests/";
    private static final String PATH_WEBVIEW_PREFIX = EXTERNAL_PREFIX + BASE_WEBVIEW_TEST_PATH;
    private static final String PATH_BLINK_PREFIX = EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH;
    private static final String GLOBAL_INTERFACE_LISTING_TEST =
            "webexposed/global-interface-listing.html";
    private static final String GLOBAL_INTERFACE_LISTING_EXPECTATION =
            "webexposed/global-interface-listing-expected.txt";

    private static final String TEST_FINISHED_SENTINEL = "TEST FINISHED";

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
    private final SettableFuture<String> mResultFuture = SettableFuture.create();

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
                        mResultFuture.setException(
                                new AssertionError(
                                        "onReceivedError: "
                                                + error.description
                                                + ", "
                                                + request.getUrl()
                                                + "\n"));
                    }

                    @Override
                    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
                        if (consoleMessage.messageLevel() == AwConsoleMessage.MESSAGE_LEVEL_LOG) {
                            mConsoleLog.append(consoleMessage.message() + "\n");
                            if (consoleMessage.message().equals(TEST_FINISHED_SENTINEL)) {
                                mResultFuture.set(mConsoleLog.toString());
                            }
                        } else {
                            mResultFuture.setException(
                                    new AssertionError(
                                            "Unexpected non-log level console message: "
                                                    + consoleMessage.message()));
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

                    // Exposes Payment APIs
                    settings.setPaymentRequestEnabled(true);

                    class SynchronousConsole {
                        @JavascriptInterface
                        public void log(String message) {
                            mConsoleLog.append(message + "\n");
                        }
                    }

                    mAwContents.addJavascriptInterface(new SynchronousConsole(), "awConsole");
                });
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        "enable-field-trial-config",
        "enable-experimental-web-platform-features",
        "enable-blink-test-features",
    })
    // Chrome branded builds don't contain fieldtrial configs.
    @Restriction(Restriction.RESTRICTION_TYPE_NON_CHROME_BRANDED)
    public void testGlobalInterfaceListingUnstable() throws Exception {
        doTestGlobalInterfaceListing("");
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({"disable-field-trial-config"})
    public void testGlobalInterfaceListingStable() throws Exception {
        doTestGlobalInterfaceListing("virtual/stable/");
    }

    public void doTestGlobalInterfaceListing(String maybeVirtual) throws Exception {
        final String repoExpectationPath =
                BASE_WEBVIEW_TEST_PATH + maybeVirtual + GLOBAL_INTERFACE_LISTING_EXPECTATION;

        final String diff =
                runTestAndDiff(
                        "file://" + PATH_BLINK_PREFIX + GLOBAL_INTERFACE_LISTING_TEST,
                        PATH_WEBVIEW_PREFIX + maybeVirtual + GLOBAL_INTERFACE_LISTING_EXPECTATION,
                        repoExpectationPath);

        if (diff.length() == 0) {
            return;
        }

        final StringBuilder message = new StringBuilder();
        message.append("\n")
                .append("WebView's set of web exposed APIs has changed and no longer matches the\n")
                .append("expectations defined in\n")
                .append("//" + repoExpectationPath + "\n")
                .append("\n")
                .append("Confirm whether these changes are deliberate. If they are, update the\n")
                .append("expectations.\n")
                .append("\n")
                .append("To update expectations, run:\n")
                .append("########### START ###########\n")
                .append(" patch -p1 <<'END_DIFF'\n")
                .append(diff)
                .append("END_DIFF\n")
                .append("############ END ############\n")
                .append("\n");
        Assert.fail(message.toString());
    }

    /**
     * Run a test and produce a unified diff of its result against its expectation.
     *
     * @param testUri URI to load to execute the test.
     * @param deviceExpectationPath On-device expectation file to compare result to.
     * @param repoExpectationPath The path to use in any output unified diff headers.
     * @returns If result matches expectation, empty string. If not, a unified diff that can be used
     *     to update the expectation file in a Chromium checkout.
     */
    private String runTestAndDiff(
            String testUri, String deviceExpectationPath, String repoExpectationPath)
            throws Exception {
        mRule.loadUrlAsync(mAwContents, testUri);

        if (isRebaseline()) {
            String result = AwActivityTestRule.waitForFuture(mResultFuture);
            writeFile(deviceExpectationPath, result);
            Log.i(
                    TAG,
                    "file: "
                            + deviceExpectationPath
                            + " --> rebaselined, length="
                            + result.length());
            return "";
        } else {
            // Read expectation in parallel with the running test.
            String expected = readFile(deviceExpectationPath);
            String result = AwActivityTestRule.waitForFuture(mResultFuture);
            return stringDiff(repoExpectationPath, expected, result);
        }
    }

    private String stringDiff(String filename, String a, String b) throws Exception {
        final List<String> aList = Arrays.asList(a.split("\\n"));
        final List<String> bList = Arrays.asList(b.split("\\n"));
        final Patch<String> patch = DiffUtils.diff(aList, bList);
        final List<String> diff =
                DiffUtils.generateUnifiedDiff(
                        "a/" + filename, "b/" + filename, aList, patch, /* contextSize= */ 3);
        final StringBuilder sb = new StringBuilder();
        for (final String line : diff) {
            sb.append(line).append("\n");
        }
        return sb.toString();
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
}
