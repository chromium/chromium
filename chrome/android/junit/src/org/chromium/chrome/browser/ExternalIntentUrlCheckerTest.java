// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.robolectric.Shadows.shadowOf;

import android.content.pm.ApplicationInfo;
import android.content.pm.ProviderInfo;
import android.os.Process;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ExternalIntentUrlChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExternalIntentUrlCheckerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExternalIntentUrlChecker.Natives mNativeMock;

    @Before
    public void setUp() {
        ExternalIntentUrlCheckerJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_InvalidUrls() {
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(null));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(GURL.emptyGURL()));
        // Note: In Robolectric with proper initialization, new GURL("invalid") is !isValid().
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(new GURL("invalid")));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_UnsafeSchemes() {
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("javascript:alert(1)")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("jar:file:///foo.jar!/bar.html")));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(new GURL("chrome://flags")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("googlechrome://navigate?url=http://example.com")));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_LocalFiles() {
        GURL fileUrl = new GURL("file:///sdcard/foo.txt");
        GURL contentUrl = new GURL("content://media/external/images/media/1");

        // allowLocalFiles = true (default)
        doReturn(true).when(mNativeMock).validateUrl(any());
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(fileUrl));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(contentUrl));

        // allowLocalFiles = false
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(fileUrl, false));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(contentUrl, false));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_NativeValidation() {
        GURL safeUrl = JUnitTestGURLs.EXAMPLE_URL;

        doReturn(true).when(mNativeMock).validateUrl(safeUrl);
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(safeUrl));

        doReturn(false).when(mNativeMock).validateUrl(safeUrl);
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(safeUrl));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_SafeExceptions() {
        doReturn(false).when(mNativeMock).validateUrl(any());

        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(new GURL("about:blank")));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(new GURL("about://blank")));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(new GURL("chrome://dino/")));
        // Note: startsWith checks
        assertFalse(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("chrome://extensions/?id=abcdef")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("chrome-native://pdf/viewer.html")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("chrome-native://pdf/link?url=content://media/external/file/1")));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_BlocksChromeFileProvider() {
        doReturn(true).when(mNativeMock).validateUrl(any());

        String packageName = ContextUtils.getApplicationContext().getPackageName();
        String encodedPackageName = packageName.replace(".", "%2E");
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("content://" + packageName + ".FileProvider/document.pdf")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL(
                                "content://"
                                        + encodedPackageName
                                        + ".FileProvider/downloads/statement.pdf")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL(
                                "content://"
                                        + encodedPackageName
                                        + ".DownloadFileProvider/download?file=report.pdf")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL(
                                "content://userinfo@"
                                        + packageName
                                        + ".FileProvider/document.pdf")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL(
                                "content://userinfo%40"
                                        + encodedPackageName
                                        + ".FileProvider/document.pdf")));
        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("content://" + packageName + "/document.pdf")));

        // Should not block legitimate external providers that start with Chrome's package name
        assertFalse(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("content://" + packageName + "cast.FileProvider/document.pdf")));
    }

    @Test
    public void testIsUnsafeExternalIntentUrl_ResolveContentUriOwner() {
        doReturn(true).when(mNativeMock).validateUrl(any());
        String packageName = ContextUtils.getApplicationContext().getPackageName();

        ProviderInfo unexportedProvider = new ProviderInfo();
        unexportedProvider.packageName = packageName;
        unexportedProvider.name = "UnexportedProvider";
        unexportedProvider.authority = "custom.unexported.provider";
        unexportedProvider.exported = false;
        unexportedProvider.applicationInfo = new ApplicationInfo();
        unexportedProvider.applicationInfo.packageName = packageName;
        unexportedProvider.applicationInfo.uid = Process.myUid();
        shadowOf(ContextUtils.getApplicationContext().getPackageManager())
                .addOrUpdateProvider(unexportedProvider);

        assertTrue(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("content://custom.unexported.provider/secret.pdf")));

        ProviderInfo exportedTestProvider = new ProviderInfo();
        exportedTestProvider.packageName = packageName;
        exportedTestProvider.name = "ExportedTestProvider";
        exportedTestProvider.authority = "custom.exported.testprovider";
        exportedTestProvider.exported = true;
        exportedTestProvider.applicationInfo = new ApplicationInfo();
        exportedTestProvider.applicationInfo.packageName = packageName;
        exportedTestProvider.applicationInfo.uid = Process.myUid();
        shadowOf(ContextUtils.getApplicationContext().getPackageManager())
                .addOrUpdateProvider(exportedTestProvider);

        assertFalse(
                ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                        new GURL("content://custom.exported.testprovider/blob.html")));
    }

    @Test
    public void testIsUnsafeExternalScheme() {
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalScheme("javascript"));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalScheme("jar"));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalScheme("googlechrome"));
        assertTrue(ExternalIntentUrlChecker.isUnsafeExternalScheme("JAVASCRIPT"));

        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme("http"));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme("https"));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme("chrome"));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme("about"));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme(null));
        assertFalse(ExternalIntentUrlChecker.isUnsafeExternalScheme(""));
    }
}
