// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.download.DownloadMetrics.DownloadOpenTarget;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;

/** Unit tests for {@link DownloadUtils} helper methods and preferred app routing. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_NEW_TAB})
public class DownloadUtilsUnitTest {
    private static final String TEST_FILE_PATH = "/path/to/downloaded_file.pdf";
    private static final String TEST_MIME_TYPE = "application/pdf";
    private static final String TEST_ORIGINAL_URL = "https://example.com/source.pdf";
    private static final String TEST_REFERRER = "https://example.com/index.html";
    private static final String TEST_GUID = "test-download-guid";
    private static final OtrProfileId TEST_OTR_PROFILE_ID = null;

    private static final String SYSTEM_RESOLVER_PACKAGE = "android";
    private static final String SYSTEM_RESOLVER_CLASS = "com.android.internal.app.ResolverActivity";
    private static final String OEM_RESOLVER_PACKAGE = "com.samsung.android.resolver";
    private static final String OEM_RESOLVER_CLASS =
            "com.samsung.android.resolver.ResolverActivity";
    private static final String EXTERNAL_APP_PACKAGE = "com.adobe.reader";
    private static final String EXTERNAL_APP_CLASS = "com.adobe.reader.AdobeReaderActivity";
    private static final String CHROME_TABBED_ACTIVITY_CLASS =
            "org.chromium.chrome.browser.ChromeTabbedActivity";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DownloadManagerService mMockDownloadManagerService;

    private Context mContext;
    private ShadowPackageManager mShadowPackageManager;
    private DownloadManagerService mOriginalDownloadManagerService;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mShadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
        mOriginalDownloadManagerService =
                DownloadManagerService.setDownloadManagerService(mMockDownloadManagerService);
    }

    @After
    public void tearDown() {
        DownloadManagerService.setDownloadManagerService(mOriginalDownloadManagerService);
    }

    private static ResolveInfo createResolveInfo(String packageName, String className) {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = className;
        return resolveInfo;
    }

    private void registerIntentHandler(Intent intent, String packageName, String className) {
        mShadowPackageManager.addResolveInfoForIntent(
                intent, createResolveInfo(packageName, className));
    }

    private DownloadOpenRequest createTestDownloadOpenRequest(String mimeType) {
        return DownloadOpenRequest.builder(mContext, TEST_FILE_PATH)
                .mimeType(mimeType)
                .downloadGuid(TEST_GUID)
                .otrProfileId(TEST_OTR_PROFILE_ID)
                .originalUrl(TEST_ORIGINAL_URL)
                .referrer(TEST_REFERRER)
                .build();
    }

    @Test
    @Feature({"Download"})
    public void testCreateViewIntent() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);

        Intent intent = DownloadUtils.createViewIntent(req);

        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(TEST_MIME_TYPE, intent.getType());
        assertNotNull(intent.getData());
        assertTrue(
                (intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        == Intent.FLAG_GRANT_READ_URI_PERMISSION);
        assertTrue(
                (intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK)
                        == Intent.FLAG_ACTIVITY_NEW_TASK);
        assertEquals(
                Uri.parse(TEST_ORIGINAL_URL),
                intent.getParcelableExtra(Intent.EXTRA_ORIGINATING_URI));
        assertEquals(Uri.parse(TEST_REFERRER), intent.getParcelableExtra(Intent.EXTRA_REFERRER));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_AndroidPackage() {
        ResolveInfo info = createResolveInfo(SYSTEM_RESOLVER_PACKAGE, SYSTEM_RESOLVER_CLASS);
        assertTrue(DownloadUtils.isSystemResolver(info));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ResolverActivityName() {
        ResolveInfo info = createResolveInfo(OEM_RESOLVER_PACKAGE, OEM_RESOLVER_CLASS);
        assertTrue(DownloadUtils.isSystemResolver(info));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_NoMatchingActivity() {
        assertTrue(DownloadUtils.isSystemResolver(null));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ExternalAppHandler() {
        ResolveInfo info = createResolveInfo(EXTERNAL_APP_PACKAGE, EXTERNAL_APP_CLASS);
        assertFalse(DownloadUtils.isSystemResolver(info));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ChromeHandler() {
        ResolveInfo info =
                createResolveInfo(mContext.getPackageName(), CHROME_TABBED_ACTIVITY_CLASS);
        assertFalse(DownloadUtils.isSystemResolver(info));
    }

    @Test
    @Feature({"Download"})
    @EnableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP})
    public void testDoOpenFile_PreferredApp_ChromeDefault() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);
        Intent targetIntent = DownloadUtils.createViewIntent(req);
        registerIntentHandler(
                targetIntent, mContext.getPackageName(), CHROME_TABBED_ACTIVITY_CLASS);
        when(mMockDownloadManagerService.isDownloadOpenableInBrowser(any(), anyBoolean()))
                .thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Download.OpenTarget", DownloadOpenTarget.CHROME_DEFAULT);

        boolean opened = DownloadUtils.openFile(req);

        assertTrue(opened);
        watcher.assertExpected();
        verify(mMockDownloadManagerService)
                .updateLastAccessTime(eq(TEST_GUID), eq(TEST_OTR_PROFILE_ID));
    }

    @Test
    @Feature({"Download"})
    @EnableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP})
    public void testDoOpenFile_PreferredApp_OtherAppDefault() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);
        Intent targetIntent = DownloadUtils.createViewIntent(req);
        registerIntentHandler(targetIntent, EXTERNAL_APP_PACKAGE, EXTERNAL_APP_CLASS);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Download.OpenTarget", DownloadOpenTarget.OTHER_APP_DEFAULT);

        boolean opened = DownloadUtils.openFile(req);

        assertTrue(opened);
        watcher.assertExpected();
        verify(mMockDownloadManagerService)
                .updateLastAccessTime(eq(TEST_GUID), eq(TEST_OTR_PROFILE_ID));
    }

    @Test
    @Feature({"Download"})
    @EnableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP})
    public void testDoOpenFile_PreferredApp_OsChooser() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);
        Intent targetIntent = DownloadUtils.createViewIntent(req);
        registerIntentHandler(targetIntent, SYSTEM_RESOLVER_PACKAGE, SYSTEM_RESOLVER_CLASS);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Download.OpenTarget", DownloadOpenTarget.OS_CHOOSER);

        boolean opened = DownloadUtils.openFile(req);

        assertTrue(opened);
        watcher.assertExpected();
        verify(mMockDownloadManagerService)
                .updateLastAccessTime(eq(TEST_GUID), eq(TEST_OTR_PROFILE_ID));
    }

    @Test
    @Feature({"Download"})
    @EnableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP})
    @DisableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_NEW_TAB})
    public void testDoOpenFile_PreferredApp_ChromeFallback() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);
        when(mMockDownloadManagerService.isDownloadOpenableInBrowser(any(), anyBoolean()))
                .thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Download.OpenTarget", DownloadOpenTarget.CHROME_FALLBACK);

        boolean opened = DownloadUtils.openFile(req);

        assertTrue(opened);
        watcher.assertExpected();
        verify(mMockDownloadManagerService)
                .updateLastAccessTime(eq(TEST_GUID), eq(TEST_OTR_PROFILE_ID));
    }

    @Test
    @Feature({"Download"})
    @EnableFeatures({ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP})
    public void testDoOpenFile_PreferredApp_NoHandlerAndCannotOpenInBrowser() {
        DownloadOpenRequest req = createTestDownloadOpenRequest("application/unsupported");
        when(mMockDownloadManagerService.isDownloadOpenableInBrowser(any(), anyBoolean()))
                .thenReturn(false);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Download.OpenTarget")
                        .build();

        boolean opened = DownloadUtils.openFile(req);

        assertFalse(opened);
        watcher.assertExpected();
    }

    @Test
    @Feature({"Download"})
    @DisableFeatures({
        ChromeFeatureList.OPEN_DOWNLOAD_IN_PREFERRED_APP,
        ChromeFeatureList.OPEN_DOWNLOAD_IN_NEW_TAB
    })
    public void testDoOpenFile_FlagDisabled_LegacyFlow() {
        DownloadOpenRequest req = createTestDownloadOpenRequest(TEST_MIME_TYPE);
        when(mMockDownloadManagerService.isDownloadOpenableInBrowser(any(), anyBoolean()))
                .thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Download.OpenTarget")
                        .build();

        boolean opened = DownloadUtils.openFile(req);

        assertTrue(opened);
        watcher.assertExpected();
        verify(mMockDownloadManagerService)
                .updateLastAccessTime(eq(TEST_GUID), eq(TEST_OTR_PROFILE_ID));
    }
}
