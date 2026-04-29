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
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;

/** Tests for {@link ContentUriUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentUriUtilsUnitTest {

    @Mock private Context mMockContext;
    @Mock private ContentResolver mMockContentResolver;
    @Mock private Cursor mMockCursor;
    @Mock private AssetFileDescriptor mMockAfd;

    private AutoCloseable mCloseable;

    @Before
    public void setUp() {
        mCloseable = MockitoAnnotations.openMocks(this);
        when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
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
}
