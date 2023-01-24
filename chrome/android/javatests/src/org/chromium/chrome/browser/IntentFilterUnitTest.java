// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;

import java.util.Arrays;

/**
 * Unit tests for Intent Filters in chrome/android/java/AndroidManifest.xml
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class IntentFilterUnitTest {
    private static final Uri HTTPS_URI = Uri.parse("https://www.example.com/index.html");
    private static final Uri ABOUT_URI = Uri.parse("about:blank");
    private static final Uri JAVASCRIPT_URI = Uri.parse("javascript:alert('hello')");
    private static final Uri CONTENT_URI = Uri.parse("content://package/path/id");
    private static final Uri HTML_URI = Uri.parse("file:///path/filename.html");
    private static final Uri MHTML_URI = Uri.parse("file:///path/to/.file/site.mhtml");
    private static final Uri WBN_URI = Uri.parse("file:///path/to/.file/site.wbn");

    // Some apps (like ShareIt) specify a file URI along with a mime type. We don't care what
    // this mime type is and trust the file extension.
    private static final String ANY_MIME = "bad/mime";

    private Intent mIntent;
    private PackageManager mPm;

    /** Parameter provider for the intent action. */
    public static class ActionParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(Intent.ACTION_VIEW).name("view"),
                    new ParameterSet().value(Intent.ACTION_SEND).name("send"));
        }
    }

    @Before
    public void setUp() {
        mPm = ContextUtils.getApplicationContext().getPackageManager();
        mIntent = new Intent();
        mIntent.setPackage(ContextUtils.getApplicationContext().getPackageName());
    }

    private void verifyIntent(boolean supported) {
        ComponentName component = mIntent.resolveActivity(mPm);
        if (supported) {
            Assert.assertNotNull(component);
        } else {
            Assert.assertNull(component);
        }
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testIgnoredMimeType(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(CONTENT_URI, "application/octet-stream");
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testHttpsUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(HTTPS_URI);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testHttpsUriWithMime(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(HTTPS_URI, "text/html");
        verifyIntent(true);
        mIntent.setDataAndType(HTTPS_URI, "text/plain");
        verifyIntent(true);
        mIntent.setDataAndType(HTTPS_URI, "application/xhtml+xml");
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testAboutUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(ABOUT_URI);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testAboutUriWithMime(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(ABOUT_URI, "text/html");
        verifyIntent(true);
        mIntent.setDataAndType(ABOUT_URI, "text/plain");
        verifyIntent(true);
        mIntent.setDataAndType(ABOUT_URI, "application/xhtml+xml");
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    // We don't support javascript URI intents.
    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testJavascriptUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(JAVASCRIPT_URI);
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testJavascriptUriWithMime(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(JAVASCRIPT_URI, "text/javascript");
        verifyIntent(false);
        mIntent.setDataAndType(JAVASCRIPT_URI, "text/html");
        verifyIntent(false);
        mIntent.setDataAndType(JAVASCRIPT_URI, "text/plain");
        verifyIntent(false);
        mIntent.setDataAndType(JAVASCRIPT_URI, "application/xhtml+xml");
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testHtmlFileUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(HTML_URI);
        verifyIntent(false);
        mIntent.setDataAndType(HTML_URI, "text/html");
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testHtmlContentUri(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(CONTENT_URI, "text/html");
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testMhtmlUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(MHTML_URI);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        // Note that calling setType() would clear the Data...
        mIntent.setDataAndType(MHTML_URI, ANY_MIME);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testContentMhtmlUri(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(CONTENT_URI, "multipart/related");
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testWbnUri(String action) {
        mIntent.setAction(action);
        mIntent.setData(WBN_URI);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        // Note that calling setType() would clear the Data...
        mIntent.setDataAndType(WBN_URI, ANY_MIME);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(action.equals(Intent.ACTION_VIEW));
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(ActionParamProvider.class)
    public void testContentWbnUri(String action) {
        mIntent.setAction(action);
        mIntent.setDataAndType(CONTENT_URI, "application/webbundle");
        verifyIntent(action.equals(Intent.ACTION_VIEW));
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }
}
