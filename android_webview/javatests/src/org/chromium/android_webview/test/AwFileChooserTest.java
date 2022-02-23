// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

// import android.support.test.InstrumentationRegistry;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.FileChooserParamsImpl;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;

/**
 * Integration tests for the WebChromeClient.onShowFileChooser method.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwFileChooserTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();
    private AwTestContainerView mTestContainerView;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwContents mAwContents;
    private AwSettings mAwSettings;
    private TestWebServer mWebServer;
    private TestAwContentsClient.ShowFileChooserHelper mShowFileChooserHelper;

    private static final String INDEX_HTML_ROUTE = "/index.html";
    private static final String sFileChoiceButtonId = "fileChooserInput";
    private File mTestFile1;
    private File mTestFile2;
    private File mTestDirectory;
    private static final String sTestDirectoryPath = PathUtils.getDataDirectory() + "/test";

    private static final String sEmptyString = new String("");

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShowFileChooserHelper = mContentsClient.getShowFileChooserHelper();
        mAwSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mAwSettings.setAllowFileAccess(true);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mTestDirectory = new File(sTestDirectoryPath);
        Assert.assertTrue(mTestDirectory.mkdirs());
        mTestFile1 = new File(mTestDirectory.getPath() + "/test1.txt");
        Assert.assertTrue(mTestFile1.createNewFile());
        mTestFile2 = new File(mTestDirectory.getPath() + "/test2.txt");
        Assert.assertTrue(mTestFile2.createNewFile());

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
        final FileChooserParamsImpl expectedBasicFileParams = new FileChooserParamsImpl(
                /*mode=*/0, /*acceptTypes=*/".txt", /*title=*/"defaultTitle",
                /*defaultFilename=*/"defaultFileName.txt", /*capture=*/false);
        final String singleFileUploadPageHtml = CommonResources.makeHtmlPageFrom(
                /*headers=*/"",
                /*body=*/"<input type='file' accept='.txt' id='" + sFileChoiceButtonId
                        + "' /><br><br>");
        final String url = mWebServer.setResponse(INDEX_HTML_ROUTE, singleFileUploadPageHtml, null);

        Assert.assertTrue(
                "Test file 1 is an empty string!", !sEmptyString.equals(mTestFile1.getPath()));
        Assert.assertNotNull("Test File 1 is null!", mTestFile1.getPath());
        mShowFileChooserHelper.setChosenFilesToUpload(
                new String[] {Uri.fromFile(mTestFile1).toString()});
        // *  Loads basic HTML page with an
        //  `<input type="file" name="..." accept="..."/>` element.
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        clickSelectFileButton();
        runTest(expectedBasicFileParams);
        final String pollFileObjectOnDom =
                "document.getElementById('" + sFileChoiceButtonId + "').files.length";
        pollJavascriptResult(pollFileObjectOnDom, "1");
    }

    @Test
    @SmallTest
    public void testShowMultipleFileChoice() throws Throwable {
        final FileChooserParamsImpl expectedMultipleFileParams = new FileChooserParamsImpl(
                /*mode=*/1, /*acceptTypes=*/".txt", /*title=*/"defaultTitle",
                /*defaultFilename=*/"defaultFileName.txt", /*capture=*/false);
        final String multipleFileUploadPageHtml = CommonResources.makeHtmlPageFrom(/*headers=*/"",
                /*body=*/"<input type='file' accept='.txt' id='" + sFileChoiceButtonId
                        + "' multiple /><br><br>");
        final String url =
                mWebServer.setResponse(INDEX_HTML_ROUTE, multipleFileUploadPageHtml, null);

        Assert.assertTrue("Test file 1 is an empty string!",
                !sEmptyString.equalsIgnoreCase(mTestFile1.getPath()));
        Assert.assertNotNull("Test File 1 is null!", mTestFile1.getPath());
        Assert.assertTrue("Test file 2 is an empty string!",
                !sEmptyString.equalsIgnoreCase(mTestFile2.getPath()));
        Assert.assertNotNull("Test File 3 is null!", mTestFile2.getPath());
        mShowFileChooserHelper.setChosenFilesToUpload(new String[] {
                Uri.fromFile(mTestFile1).toString(), Uri.fromFile(mTestFile2).toString()});
        // *  Loads basic HTML page with an
        //  `<input type="file" name="..." accept="..." multiple/>` element.
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        clickSelectFileButton();
        runTest(expectedMultipleFileParams);
        final String pollFileObjectOnDom =
                "document.getElementById('" + sFileChoiceButtonId + "').files.length";
        pollJavascriptResult(pollFileObjectOnDom, "2");
    }

    /**
     *  Simulates user clicking Choose File button.
     */
    private void clickSelectFileButton() throws Exception {
        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), sFileChoiceButtonId);
    }

    /**
     *  Verifies the showFileChooser is utilizing
     *  the appropriate mode for the given test type.
     */
    private void runTest(FileChooserParamsImpl expectedParams) throws Exception {
        int callCount = mShowFileChooserHelper.getCallCount();
        mShowFileChooserHelper.waitForCallback(callCount);
        final FileChooserParamsImpl params = mShowFileChooserHelper.getFileParams();
        Assert.assertEquals(expectedParams.getMode(), params.getMode());
        Assert.assertEquals(expectedParams.getAcceptTypesString(), params.getAcceptTypesString());
    }

    /**
     *  Wait until the expected string matches what
     *  value is in the DOM returned  by the JavaScript code
     */
    private void pollJavascriptResult(String script, String expectedResult) throws Throwable {
        AwActivityTestRule.pollInstrumentationThread(() -> {
            try {
                return expectedResult.equals(executeJavaScriptAndWaitForResult(script));
            } catch (Throwable e) {
                return false;
            }
        });
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mActivityTestRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }
}
