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
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for Intent Filters in chrome/android/java/AndroidManifest.xml */
@RunWith(BaseJUnit4ClassRunner.class)
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

    @Before
    public void setUp() {
        mPm = ContextUtils.getApplicationContext().getPackageManager();
        mIntent = new Intent(Intent.ACTION_VIEW);
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
    public void testIgnoredMimeType() {
        mIntent.setDataAndType(CONTENT_URI, "application/octet-stream");
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testHttpsUri() {
        mIntent.setData(HTTPS_URI);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    public void testHttpsUriWithMime() {
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
    public void testAboutUri() {
        mIntent.setData(ABOUT_URI);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    public void testAboutUriWithMime() {
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
    public void testJavascriptUri() {
        mIntent.setData(JAVASCRIPT_URI);
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testJavascriptUriWithMime() {
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
    public void testHtmlFileUri() {
        mIntent.setData(HTML_URI);
        verifyIntent(false);
        mIntent.setDataAndType(HTML_URI, "text/html");
        verifyIntent(false);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testHtmlContentUri() {
        mIntent.setDataAndType(CONTENT_URI, "text/html");
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testMhtmlUri() {
        mIntent.setData(MHTML_URI);
        verifyIntent(true);
        // Note that calling setType() would clear the Data...
        mIntent.setDataAndType(MHTML_URI, ANY_MIME);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    public void testContentMhtmlUri() {
        mIntent.setDataAndType(CONTENT_URI, "multipart/related");
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testWbnUri() {
        mIntent.setData(WBN_URI);
        verifyIntent(true);
        // Note that calling setType() would clear the Data...
        mIntent.setDataAndType(WBN_URI, ANY_MIME);
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(true);
    }

    @Test
    @SmallTest
    public void testContentWbnUri() {
        mIntent.setDataAndType(CONTENT_URI, "application/webbundle");
        verifyIntent(true);
        mIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        verifyIntent(false);
    }

    @Test
    @SmallTest
    public void testShareIntent() {
        mIntent.setType("text/plain");
        verifyIntent(false);
        mIntent.setAction(Intent.ACTION_SEND);
        verifyIntent(true);
    }
}
