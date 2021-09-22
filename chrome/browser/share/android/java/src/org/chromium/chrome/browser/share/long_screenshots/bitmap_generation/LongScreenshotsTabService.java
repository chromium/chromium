// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Rect;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * The Java-side implementations of long_screenshots_tab_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation. This class provides the required functionalities for
 * capturing the Paint Preview representation of a tab.
 */
@JNINamespace("long_screenshots")
public class LongScreenshotsTabService implements NativePaintPreviewServiceProvider {
    /** Interface used for notifying in the event of navigation to a URL. */
    public interface CaptureProcessor {
        void processCapturedTab(long nativeCaptureResultPtr, @Status int status);
    }

    private CaptureProcessor mCaptureProcessor;

    private long mNativeLongScreenshotsTabService;

    @CalledByNative
    private LongScreenshotsTabService(long nativeLongScreenshotsTabService) {
        mNativeLongScreenshotsTabService = nativeLongScreenshotsTabService;
    }

    // Set the object responsible for processing the results after a tab capture.
    public void setCaptureProcessor(CaptureProcessor captureProcessor) {
        mCaptureProcessor = captureProcessor;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeLongScreenshotsTabService = 0;
    }

    @CalledByNative
    private void processCaptureTabStatus(@Status int status) {
        if (mCaptureProcessor != null) {
            mCaptureProcessor.processCapturedTab(0, status);
        }
    }

    @CalledByNative
    private void processPaintPreviewResponse(long nativeCaptureResultPtr) {
        if (mCaptureProcessor == null) {
            // TODO(tgupta): return an error here.
            releaseNativeCaptureResultPtr(nativeCaptureResultPtr);
            return;
        }

        mCaptureProcessor.processCapturedTab(nativeCaptureResultPtr, Status.OK);
    }

    public void captureTab(Tab tab, Rect clipRect, boolean inMemory) {
        if (mNativeLongScreenshotsTabService == 0) {
            processCaptureTabStatus(Status.NATIVE_SERVICE_NOT_INITIALIZED);
        }

        if (tab.getWebContents() == null) {
            processCaptureTabStatus(Status.WEB_CONTENTS_GONE);
        }

        LongScreenshotsTabServiceJni.get().captureTabAndroid(mNativeLongScreenshotsTabService,
                tab.getId(), tab.getUrl(), tab.getWebContents(), clipRect.left, clipRect.top,
                clipRect.right, clipRect.height(), inMemory);
    }

    public void longScreenshotsClosed() {
        if (mNativeLongScreenshotsTabService == 0) {
            return;
        }
        LongScreenshotsTabServiceJni.get().longScreenshotsClosedAndroid(
                mNativeLongScreenshotsTabService);
    }

    @Override
    public long getNativeBaseService() {
        return mNativeLongScreenshotsTabService;
    }

    public void releaseNativeCaptureResultPtr(long nativeCaptureResultPtr) {
        if (nativeCaptureResultPtr == 0) return;

        LongScreenshotsTabServiceJni.get().releaseCaptureResultPtr(nativeCaptureResultPtr);
    }

    @NativeMethods
    interface Natives {
        void captureTabAndroid(long nativeLongScreenshotsTabService, int tabId, GURL url,
                WebContents webContents, int clipX, int clipY, int clipWidth, int clipHeight,
                boolean inMemory);
        void longScreenshotsClosedAndroid(long nativeLongScreenshotsTabService);
        void releaseCaptureResultPtr(long captureResultPtr);
    }
}
