// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link DownloadUtils} helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadUtilsUnitTest {
    private static final String TEST_FILE_PATH = "/path/to/downloaded_file.pdf";
    private static final String TEST_MIME_TYPE = "application/pdf";
    private static final String TEST_ORIGINAL_URL = "https://example.com/source.pdf";
    private static final String TEST_REFERRER = "https://example.com/index.html";

    private static final String SYSTEM_RESOLVER_PACKAGE = "android";
    private static final String SYSTEM_RESOLVER_CLASS = "com.android.internal.app.ResolverActivity";
    private static final String OEM_RESOLVER_PACKAGE = "com.samsung.android.resolver";
    private static final String OEM_RESOLVER_CLASS =
            "com.samsung.android.resolver.ResolverActivity";
    private static final String EXTERNAL_APP_PACKAGE = "com.adobe.reader";
    private static final String EXTERNAL_APP_CLASS = "com.adobe.reader.AdobeReaderActivity";
    private static final String CHROME_TABBED_ACTIVITY_CLASS =
            "org.chromium.chrome.browser.ChromeTabbedActivity";

    private Context mContext;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mShadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
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

    private static Intent createTestViewIntent(String mimeType) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(Uri.parse("content://downloads/1"), mimeType);
        return intent;
    }

    @Test
    @Feature({"Download"})
    public void testCreateViewIntent() {
        DownloadOpenRequest req =
                DownloadOpenRequest.builder(mContext, TEST_FILE_PATH)
                        .mimeType(TEST_MIME_TYPE)
                        .originalUrl(TEST_ORIGINAL_URL)
                        .referrer(TEST_REFERRER)
                        .build();

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
        Intent intent = createTestViewIntent(TEST_MIME_TYPE);
        registerIntentHandler(intent, SYSTEM_RESOLVER_PACKAGE, SYSTEM_RESOLVER_CLASS);

        assertTrue(DownloadUtils.isSystemResolver(mContext, intent));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ResolverActivityName() {
        Intent intent = createTestViewIntent(TEST_MIME_TYPE);
        registerIntentHandler(intent, OEM_RESOLVER_PACKAGE, OEM_RESOLVER_CLASS);

        assertTrue(DownloadUtils.isSystemResolver(mContext, intent));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_NoMatchingActivity() {
        Intent intent = createTestViewIntent("application/unsupported");

        assertTrue(DownloadUtils.isSystemResolver(mContext, intent));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ExternalAppHandler() {
        Intent intent = createTestViewIntent(TEST_MIME_TYPE);
        registerIntentHandler(intent, EXTERNAL_APP_PACKAGE, EXTERNAL_APP_CLASS);

        assertFalse(DownloadUtils.isSystemResolver(mContext, intent));
    }

    @Test
    @Feature({"Download"})
    public void testIsSystemResolver_ChromeHandler() {
        Intent intent = createTestViewIntent(TEST_MIME_TYPE);
        registerIntentHandler(intent, mContext.getPackageName(), CHROME_TABBED_ACTIVITY_CLASS);

        assertFalse(DownloadUtils.isSystemResolver(mContext, intent));
    }
}
