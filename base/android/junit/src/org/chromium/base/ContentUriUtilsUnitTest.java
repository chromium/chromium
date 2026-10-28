// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Process;
import android.provider.OpenableColumns;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;

/** Tests for {@link ContentUriUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContentUriUtilsUnitTest {

    @Mock private Context mMockContext;
    @Mock private ContentResolver mMockContentResolver;
    @Mock private PackageManager mMockPackageManager;
    @Mock private Cursor mMockCursor;
    @Mock private AssetFileDescriptor mMockAfd;

    private AutoCloseable mCloseable;

    @Before
    public void setUp() {
        mCloseable = MockitoAnnotations.openMocks(this);
        when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
        when(mMockContext.getPackageName()).thenReturn("org.chromium.test");
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        ContextUtils.initApplicationContextForTests(mMockContext);
    }

    @After
    public void tearDown() throws Exception {
        if (mCloseable != null) {
            mCloseable.close();
        }
    }

    @Test
    public void testIsOpenableFile_ValidFile() {
        Uri uri = Uri.parse("content://tmp/test/file.jpg");
        when(mMockContentResolver.query(uri, null, null, null, null)).thenReturn(mMockCursor);
        when(mMockCursor.moveToFirst()).thenReturn(true);
        when(mMockCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)).thenReturn(0);
        when(mMockCursor.getColumnIndex(OpenableColumns.SIZE)).thenReturn(1);

        assertTrue(ContentUriUtils.isOpenableFile(uri));
    }

    @Test
    public void testIsOpenableFile_NotFile() {
        Uri uri = Uri.parse("content://tmp/test/stream");
        when(mMockContentResolver.query(uri, null, null, null, null)).thenReturn(mMockCursor);
        when(mMockCursor.moveToFirst()).thenReturn(true);
        when(mMockCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)).thenReturn(-1);

        assertFalse(ContentUriUtils.isOpenableFile(uri));
    }

    @Test
    public void testReadTextFromUri_NullUri() {
        assertNull(ContentUriUtils.readTextFromUri(null, "text/html"));
    }

    @Test
    public void testReadTextFromUri_Success() throws Exception {
        Uri uri = Uri.parse("content://tmp/test/stream");
        String mimeType = "text/html";
        String content = "<html><body>Hello</body></html>";

        when(mMockContentResolver.getStreamTypes(uri, mimeType))
                .thenReturn(new String[] {mimeType});
        when(mMockContentResolver.openTypedAssetFileDescriptor(uri, mimeType, null))
                .thenReturn(mMockAfd);

        File tempFile = File.createTempFile("test", "html");
        tempFile.deleteOnExit();
        try (FileWriter writer = new FileWriter(tempFile)) {
            writer.write(content);
        }
        FileInputStream fileInputStream = new FileInputStream(tempFile);
        when(mMockAfd.createInputStream()).thenReturn(fileInputStream);

        assertEquals(content, ContentUriUtils.readTextFromUri(uri, mimeType));
    }

    @Test
    public void testReadTextFromUri_MimeTypeMismatch() throws Exception {
        Uri uri = Uri.parse("content://tmp/test/stream");
        String requestedType = "text/html";
        String actualType = "text/plain";

        when(mMockContentResolver.getStreamTypes(uri, requestedType))
                .thenReturn(new String[] {actualType});

        assertNull(ContentUriUtils.readTextFromUri(uri, requestedType));
    }

    private ProviderInfo mockProvider(String authority, String packageName, int uid) {
        ProviderInfo provider = new ProviderInfo();
        provider.packageName = packageName;
        provider.applicationInfo = new ApplicationInfo();
        provider.applicationInfo.uid = uid;
        when(mMockPackageManager.resolveContentProvider(authority, 0)).thenReturn(provider);
        return provider;
    }

    @Test
    public void testResolveContentUriOwner_UsesProcessUidNotContextApplicationUid() {
        ApplicationInfo contextApplication = new ApplicationInfo();
        contextApplication.uid = Process.myUid() + 1;
        when(mMockContext.getApplicationInfo()).thenReturn(contextApplication);
        mockProvider("shared.uid", "org.chromium.other_package", Process.myUid());

        assertEquals(
                ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://shared.uid/path")));
    }

    @Test
    public void testResolveContentUriOwner_RejectsSamePackageWithDifferentUid() {
        mockProvider("different.uid", "org.chromium.test", Process.myUid() + 1);

        assertEquals(
                ContentUriOwner.OTHER_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://different.uid/path")));
    }

    @Test
    public void testResolveContentUriOwner_WithoutProviderApplicationInfo() {
        ProviderInfo provider = new ProviderInfo();
        provider.packageName = "org.chromium.test";
        when(mMockPackageManager.resolveContentProvider("missing.info", 0)).thenReturn(provider);

        assertEquals(
                ContentUriOwner.OTHER_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://missing.info/path")));
        assertEquals(
                ContentUriOwner.UNRESOLVED,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://unresolved/path")));
    }

    @Test
    public void testResolveContentUriOwner_OpaqueUriWithAuthorityInStringForm() {
        mockProvider("shared.uid", "org.chromium.other_package", Process.myUid());

        // An OpaqueUri reports a null authority even though its string form encodes one. The check
        // must recover the authority from the string form; otherwise a caller could smuggle one of
        // this app's own content URIs past it.
        Uri opaque =
                new Uri.Builder().scheme("content").encodedOpaquePart("//shared.uid/path").build();
        assertNull(opaque.getAuthority());
        assertEquals("content://shared.uid/path", opaque.toString());

        assertEquals(
                ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(opaque));
    }

    @Test
    public void testResolveContentUriOwner_OpaqueUriWithoutAuthority() {
        // An OpaqueUri whose string form has no authority (e.g. "content:foo") must resolve to
        // UNRESOLVED without recursing.
        Uri opaque = new Uri.Builder().scheme("content").encodedOpaquePart("foo").build();
        assertNull(opaque.getAuthority());

        assertEquals(ContentUriOwner.UNRESOLVED, ContentUriUtils.resolveContentUriOwner(opaque));
    }

    @Test
    public void testResolveContentUriOwner_ExportedFromCurrentApp() {
        mockProvider("exported", "org.chromium.test", Process.myUid()).exported = true;

        assertEquals(
                ContentUriOwner.EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://exported/path")));
    }

    @Test
    public void testResolveContentUriOwner_NonExportedFromCurrentApp() {
        mockProvider("unexported", "org.chromium.test", Process.myUid());

        assertEquals(
                ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://unexported/path")));
    }

    @Test
    public void testResolveContentUriOwner_OtherApp() {
        mockProvider("different.uid", "org.chromium.other_package", Process.myUid() + 1);

        assertEquals(
                ContentUriOwner.OTHER_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://different.uid/path")));
    }

    @Test
    public void testResolveContentUriOwner_Unresolved() {
        assertEquals(
                ContentUriOwner.UNRESOLVED,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://unresolved/path")));
    }

    @Test
    public void testResolveContentUriOwner_UserQualifiedAuthorityUsesBareAuthority() {
        mockProvider("exported", "org.chromium.test", Process.myUid()).exported = true;
        mockProvider("unexported", "org.chromium.test", Process.myUid());

        assertEquals(
                ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(Uri.parse("content://10@unexported/path")));
        assertEquals(
                ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP,
                ContentUriUtils.resolveContentUriOwner(
                        Uri.parse("content://10@exported@unexported/path")));
    }
}
