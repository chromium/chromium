// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_capture;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.components.content_capture.PlatformContentCaptureController;

/** Unit tests for the ContentCaptureHistoryDeletionObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentCaptureHistoryDeletionObserverTest {
    @Mock PlatformContentCaptureController mContentCaptureController;
    @Mock HistoryDeletionInfo mHistoryDeletionInfo;

    ContentCaptureHistoryDeletionObserver mContentCaptureHistoryDeletionObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContentCaptureHistoryDeletionObserver =
                new ContentCaptureHistoryDeletionObserver(() -> mContentCaptureController);
    }

    @Test
    public void clearAllData_FromSpecificTimeRange() {
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeForAllTime();
        doReturn(true).when(mHistoryDeletionInfo).isTimeRangeValid();

        mContentCaptureHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mContentCaptureController).clearAllContentCaptureData();
    }

    @Test
    public void clearAllData_ForAllTime() {
        doReturn(true).when(mHistoryDeletionInfo).isTimeRangeForAllTime();
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeValid();

        mContentCaptureHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mContentCaptureController).clearAllContentCaptureData();
    }

    @Test
    public void clearAllData_ForSpecficURLs() {
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeForAllTime();
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeValid();
        String[] urls = new String[] {"one", "two", "three"};
        doReturn(urls).when(mHistoryDeletionInfo).getDeletedURLs();

        mContentCaptureHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mContentCaptureController).clearContentCaptureDataForURLs(urls);
    }

    @Test
    public void clearAllData_ThrowsRuntimeException() {
        doThrow(RuntimeException.class)
                .when(mContentCaptureController)
                .clearContentCaptureDataForURLs(any());
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeForAllTime();
        doReturn(false).when(mHistoryDeletionInfo).isTimeRangeValid();
        String[] urls = new String[] {"one", "two", "three"};
        doReturn(urls).when(mHistoryDeletionInfo).getDeletedURLs();

        // Runtime exception should be caught and logged.
        mContentCaptureHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mContentCaptureController).clearContentCaptureDataForURLs(urls);
        verify(mContentCaptureController).clearAllContentCaptureData();
    }
}
