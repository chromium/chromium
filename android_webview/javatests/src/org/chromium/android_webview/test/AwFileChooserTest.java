// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Intent;
import android.net.Uri;
import android.webkit.WebChromeClient;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.FileChooserParamsImpl;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;
import java.util.concurrent.TimeoutException;

/** Integration tests for the WebChromeClient.onShowFileChooser method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Shared dependencies among the tests cause conflicts during batch testing.")
public class AwFileChooserTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;
    private AwTestContainerView mTestContainerView;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwContents mAwContents;
    private AwSettings mAwSettings;
    private TestWebServer mWebServer;
    private TestAwContentsClient.ShowFileChooserHelper mShowFileChooserHelper;

    private static final String INDEX_HTML_ROUTE = "/index.html";
    private static final String FILE_CHOICE_BUTTON_ID = "fileChooserInput";
    private File mTestFile1;
    private File mTestFile2;
    private File mTestDirectory;
    private static final String TEST_DIRECTORY_PATH = PathUtils.getDataDirectory() + "/test";

    private static final String TAG = "AwFileChooserTest";

    public AwFileChooserTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShowFileChooserHelper = mContentsClient.getShowFileChooserHelper();
        mAwSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mAwSettings.setAllowFileAccess(true);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mTestDirectory = new File(TEST_DIRECTORY_PATH);
        Assert.assertTrue(mTestDirectory.mkdirs());
        mTestFile1 = new File(mTestDirectory.getPath() + "/test1.txt");
        Assert.assertTrue(mTestFile1.createNewFile());
        mTestFile2 = new File(mTestDirectory.getPath() + "/test2.txt");
        Assert.assertTrue(mTestFile2.createNewFile());

        Assert.assertTrue(
                "Test file 1 is an empty string!", !"".equalsIgnoreCase(mTestFile1.getPath()));
        Assert.assertNotNull("Test File 1 is null!", mTestFile1.getPath());
        Assert.assertTrue(
                "Test file 2 is an empty string!", !"".equalsIgnoreCase(mTestFile2.getPath()));
        Assert.assertNotNull("Test File 2 is null!", mTestFile2.getPath());

        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
        Assert.assertTrue(FileUtils.recursivelyDeleteFile(mTestDirectory, FileUtils.DELETE_ALL));
    }

    @Test
    @SmallTest
    public void testShowSingleFileChoice() throws Throwable {
        final String singleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' /><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("1");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(WebChromeClient.FileChooserParams.MODE_OPEN, params.getMode());
    }

    @Test
    @SmallTest
    public void testShowMultipleFileChoice() throws Throwable {
        final String multipleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' multiple/><br><br>");
        final String url =
                mWebServer.setResponse(INDEX_HTML_ROUTE, multipleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {
                    Uri.fromFile(mTestFile1).toString(), Uri.fromFile(mTestFile2).toString()
                });
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("2");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE, params.getMode());
    }

    @Test
    @SmallTest
    public void testShowDirectoryChoice() throws Throwable {
        final String multipleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' webkitdirectory ><br><br>");
        final String url =
                mWebServer.setResponse(INDEX_HTML_ROUTE, multipleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {
                    Uri.fromFile(mTestFile1).toString(), Uri.fromFile(mTestFile2).toString()
                });
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("2");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        // Upload folder is not currently supported in webview
        // As a workaround, it is treated as OPEN_MULTIPLE_MODE
        Assert.assertEquals(WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE, params.getMode());
    }

    @Test
    @SmallTest
    public void testAcceptTypes() throws Throwable {
        final String[] expectedIntentExtraTypes = {"application/pdf", "text/plain", "image/png"};
        final String expectedIntentType = expectedIntentExtraTypes[0];
        final String expectedAcceptTypesString = ".pdf,.txt,.png";
        final String singleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='"
                                + expectedAcceptTypesString
                                + "' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' /><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("1");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(expectedAcceptTypesString, params.getAcceptTypesString());

        // Testing FileChooserParamsImpl.createIntent API
        // Verifies that the file choice type and the extra types are set properly
        Intent i = params.createIntent();
        Assert.assertEquals(
                i.getStringArrayExtra(Intent.EXTRA_MIME_TYPES), expectedIntentExtraTypes);
        Assert.assertEquals(i.getType(), expectedIntentType);
    }

    @Test
    @SmallTest
    public void testIsCaptureEnabled() throws Throwable {
        final boolean captureEnabled = true;
        final String singleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' capture/><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("1");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(captureEnabled, params.isCaptureEnabled());
    }

    @Test
    @SmallTest
    public void testIsCaptureDisabled() throws Throwable {
        final boolean captureEnabled = false;
        final String singleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' /><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButtonAndWaitForCallback("1");
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(captureEnabled, params.isCaptureEnabled());
    }

    @Test
    @SmallTest
    public void testInvalidUriIsCanceled() throws Throwable {
        final String singleFileUploadPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<input type='file' accept='.txt' id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' /><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                // Using one real Uri and one invalid Uri because
                // ActivityTestRule#executeJavaScriptAndWaitForResult
                // waits for a DOM change to occur In order for the DOM to change, we must provide
                // at least one real Uri
                new String[] {Uri.fromFile(mTestFile1).toString(), "/BadUri/ThatIsnt/Valid"});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        clickSelectFileButtonAndWaitForCallback("1");

        // Using two real Uris and one invalid Uri
        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {
                    Uri.fromFile(mTestFile1).toString(),
                    Uri.fromFile(mTestFile2).toString(),
                    "/BadUri/ThatIsnt/Valid"
                });
        clickSelectFileButtonAndWaitForCallback("2");
    }

    public void fileSystemAccessCancelled(String function) throws Throwable {
        final String saveFilePickerPageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<div id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' onclick='"
                                + function
                                + ".catch(e => window.result = e.name)'/>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, saveFilePickerPageHtml, null);

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButton();
        pollJavascriptResult("window.result", "\"AbortError\"");
        Assert.assertEquals(0, mShowFileChooserHelper.getCallCount());
    }

    private void fileSystemAccessOk(String function, int expectedMode) throws Throwable {
        final String pageHtml =
                CommonResources.makeHtmlPageFrom(
                        /* headers= */ "",
                        /* body= */ "<div id='"
                                + FILE_CHOICE_BUTTON_ID
                                + "' onclick='"
                                + function
                                + ".then(() => window.done = true)"
                                + ".catch(() => window.done = true)'/>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, pageHtml, null);

        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        clickSelectFileButton();
        pollJavascriptResult("window.done", "true");
        Assert.assertEquals(1, mShowFileChooserHelper.getCallCount());
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(expectedMode, params.getMode());
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL)
    @DisableFeatures(AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS)
    public void testFileSystemAccessBeforeTargetSdkOpenFile() throws Throwable {
        fileSystemAccessOk("showOpenFilePicker()", WebChromeClient.FileChooserParams.MODE_OPEN);
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL)
    @DisableFeatures(AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS)
    public void testFileSystemAccessBeforeTargetSdkSaveFileCancelled() throws Throwable {
        fileSystemAccessCancelled("showSaveFilePicker()");
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL)
    @DisableFeatures(AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS)
    public void testFileSystemAccessBeforeTargetSdkDirectoryCancelled() throws Throwable {
        fileSystemAccessCancelled("showDirectoryPicker()");
    }

    @Test
    @SmallTest
    @EnableFeatures({BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL, AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS})
    public void testFileSystemAccessOpenFile() throws Throwable {
        fileSystemAccessOk("showOpenFilePicker()", WebChromeClient.FileChooserParams.MODE_OPEN);
    }

    @Test
    @SmallTest
    @EnableFeatures({BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL, AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS})
    public void testFileSystemAccessSaveFile() throws Throwable {
        fileSystemAccessOk("showSaveFilePicker()", WebChromeClient.FileChooserParams.MODE_SAVE);
    }

    @Test
    @SmallTest
    @EnableFeatures({BlinkFeatures.FILE_SYSTEM_ACCESS_LOCAL, AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS})
    public void testFileSystemAccessDirectory() throws Throwable {
        fileSystemAccessOk(
                "showDirectoryPicker()", WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE);
    }

    /** Simulates user clicking Choose File button. */
    private void clickSelectFileButton() throws Exception {
        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), FILE_CHOICE_BUTTON_ID);
    }

    /**
     *  Wait until the expected string matches what
     *  value is in the DOM returned  by the JavaScript code
     */
    private void pollJavascriptResult(String script, String expectedResult) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        return expectedResult.equals(executeJavaScriptAndWaitForResult(script));
                    } catch (Throwable t) {
                        throw new RuntimeException(t);
                    }
                });
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mActivityTestRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private void clickSelectFileButtonAndWaitForCallback(String expectedNumberOfFiles)
            throws TimeoutException, Exception, Throwable {
        int callCount = mShowFileChooserHelper.getCallCount();
        clickSelectFileButton();
        mShowFileChooserHelper.waitForCallback(callCount);

        final String pollFileObjectOnDom =
                "document.getElementById('" + FILE_CHOICE_BUTTON_ID + "').files.length";
        pollJavascriptResult(pollFileObjectOnDom, expectedNumberOfFiles);
    }
}
