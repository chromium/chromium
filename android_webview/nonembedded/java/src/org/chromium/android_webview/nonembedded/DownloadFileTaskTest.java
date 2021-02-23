// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Same package as DownloadFileTask to access DownloadFileTask.Natives.
package org.chromium.android_webview.nonembedded;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.url.GURL;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.util.concurrent.TimeoutException;

/** Test DownloadFileTask. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadFileTaskTest {
    private HttpURLConnection mConnection;

    private Context mContext;
    private File mTempDirectory;

    @Rule
    public JniMocker jniMocker = new JniMocker();
    @Mock
    private DownloadFileTask.Natives mNativeMock;

    @Before
    public void setUp() throws IOException {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(DownloadFileTaskJni.TEST_HOOKS, mNativeMock);

        mContext = ContextUtils.getApplicationContext();
        mTempDirectory = new File(mContext.getFilesDir(), "tmp/");
        Assert.assertTrue(mTempDirectory.exists() || mTempDirectory.mkdirs());

        mConnection = mock(HttpURLConnection.class);
    }

    @After
    public void tearDown() throws IOException {
        Assert.assertTrue("Failed to cleanup temporary test files",
                FileUtils.recursivelyDeleteFile(mTempDirectory, null));
    }

    @Test
    @MediumTest
    public void testDownloadToFile() throws IOException, TimeoutException {
        File file = new File(mTempDirectory, "downloadedContents.txt");
        Assert.assertTrue(file.exists() || file.createNewFile());

        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("Content-Length")).thenReturn("4");
        when(mConnection.getInputStream())
                .thenReturn(new ByteArrayInputStream(ApiCompatibilityUtils.getBytesUtf8("1234")));

        DownloadFileTask.downloadToFile(mConnection, /** nativeDownloadFileTask= */ 0,
                /** mainTaskRunner= */ 0, mock(GURL.class), file.getAbsolutePath());
        verify(mNativeMock).callResponseStartedCallback(0, 0, 200, 4);
        verify(mNativeMock).callProgressCallback(0, 0, 4);
        verify(mNativeMock).callDownloadToFileCompleteCallback(0, 0, 0, 4);
    }
}
