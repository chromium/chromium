// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.OtrProfileId;

/** Unit tests for {@link DownloadOpenRequest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadOpenRequestTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
    }

    @Test
    @Feature({"Download"})
    public void testBuilderWithRequiredFields() {
        String filePath = "/path/to/file.pdf";

        DownloadOpenRequest request = DownloadOpenRequest.builder(mContext, filePath).build();

        assertEquals(filePath, request.mFilePath);
        assertEquals(mContext, request.mContext);
        assertNull(request.mMimeType);
        assertNull(request.mDownloadGuid);
        assertNull(request.mOtrProfileId);
        assertNull(request.mOriginalUrl);
        assertNull(request.mReferrer);
        assertEquals(0, request.mSource);
        assertNull(request.mFileName);
    }

    @Test
    @Feature({"Download"})
    public void testBuilderWithAllFields() {
        String filePath = "/path/to/file.pdf";
        String mimeType = "application/pdf";
        String downloadGuid = "test-guid-123";
        OtrProfileId otrProfileId = OtrProfileId.getPrimaryOtrProfileId();
        String originalUrl = "https://example.com/file.pdf";
        String referrer = "https://example.com";
        @DownloadOpenSource int source = DownloadOpenSource.NOTIFICATION;
        String fileName = "file.pdf";

        DownloadOpenRequest request =
                DownloadOpenRequest.builder(mContext, filePath)
                        .mimeType(mimeType)
                        .downloadGuid(downloadGuid)
                        .otrProfileId(otrProfileId)
                        .originalUrl(originalUrl)
                        .referrer(referrer)
                        .source(source)
                        .fileName(fileName)
                        .build();

        assertEquals(filePath, request.mFilePath);
        assertEquals(mContext, request.mContext);
        assertEquals(mimeType, request.mMimeType);
        assertEquals(downloadGuid, request.mDownloadGuid);
        assertEquals(otrProfileId, request.mOtrProfileId);
        assertEquals(originalUrl, request.mOriginalUrl);
        assertEquals(referrer, request.mReferrer);
        assertEquals(source, request.mSource);
        assertEquals(fileName, request.mFileName);
    }

    @Test
    @Feature({"Download"})
    public void testBuilderChaining() {
        String filePath = "/path/to/video.mp4";
        String mimeType = "video/mp4";
        String fileName = "video.mp4";

        DownloadOpenRequest request =
                DownloadOpenRequest.builder(mContext, filePath)
                        .mimeType(mimeType)
                        .fileName(fileName)
                        .source(DownloadOpenSource.DOWNLOAD_HOME)
                        .build();

        assertEquals(filePath, request.mFilePath);
        assertEquals(mimeType, request.mMimeType);
        assertEquals(fileName, request.mFileName);
        assertEquals(DownloadOpenSource.DOWNLOAD_HOME, request.mSource);
    }

    @Test
    @Feature({"Download"})
    public void testBuilderWithNullOptionalFields() {
        String filePath = "/path/to/file.txt";

        DownloadOpenRequest request =
                DownloadOpenRequest.builder(mContext, filePath)
                        .mimeType(null)
                        .downloadGuid(null)
                        .otrProfileId(null)
                        .originalUrl(null)
                        .referrer(null)
                        .fileName(null)
                        .build();

        assertEquals(filePath, request.mFilePath);
        assertNull(request.mMimeType);
        assertNull(request.mDownloadGuid);
        assertNull(request.mOtrProfileId);
        assertNull(request.mOriginalUrl);
        assertNull(request.mReferrer);
        assertNull(request.mFileName);
    }

    @Test
    @Feature({"Download"})
    public void testBuilderOverwriteValues() {
        String filePath = "/path/to/file.pdf";
        String initialMimeType = "text/plain";
        String finalMimeType = "application/pdf";

        DownloadOpenRequest request =
                DownloadOpenRequest.builder(mContext, filePath)
                        .mimeType(initialMimeType)
                        .mimeType(finalMimeType)
                        .build();

        assertEquals(finalMimeType, request.mMimeType);
    }

    @Test
    @Feature({"Download"})
    public void testDifferentDownloadOpenSources() {
        String filePath = "/path/to/file.pdf";

        DownloadOpenRequest autoOpenRequest =
                DownloadOpenRequest.builder(mContext, filePath)
                        .source(DownloadOpenSource.AUTO_OPEN)
                        .build();
        assertEquals(DownloadOpenSource.AUTO_OPEN, autoOpenRequest.mSource);

        DownloadOpenRequest notificationRequest =
                DownloadOpenRequest.builder(mContext, filePath)
                        .source(DownloadOpenSource.NOTIFICATION)
                        .build();
        assertEquals(DownloadOpenSource.NOTIFICATION, notificationRequest.mSource);

        DownloadOpenRequest menuRequest =
                DownloadOpenRequest.builder(mContext, filePath)
                        .source(DownloadOpenSource.MENU)
                        .build();
        assertEquals(DownloadOpenSource.MENU, menuRequest.mSource);
    }
}
