// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Rect;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java-side implementations of long_screenshots_tab_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation. This class provides the required functionalities for
 * capturing the Paint Preview representation of a tab.
 */
@JNINamespace("long_screenshots")
public class LongScreenshotsTabService implements NativePaintPreviewServiceProvider {
    /** Interface used for notifying in the event of navigation to a URL. */
    public interface CaptureProcessor {
        void processCapturedTab(PaintPreviewProto response, @Status int status);
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
            mCaptureProcessor.processCapturedTab(null, status);
        }
    }

    @CalledByNative
    private void processPaintPreviewResponse(byte[] paintPreviewProtoResponse) {
        if (mCaptureProcessor == null) {
            // TODO(tgupta): return an error here.
            return;
        }

        PaintPreviewProto response;
        try {
            response = PaintPreviewProto.parseFrom(paintPreviewProtoResponse);
        } catch (Exception e) {
            processCaptureTabStatus(Status.PROTO_DESERIALIZATION_FAILED);
            return;
        }
        mCaptureProcessor.processCapturedTab(response, Status.OK);
    }

    public void captureTab(Tab tab, Rect clipRect) {
        if (mNativeLongScreenshotsTabService == 0) {
            processCaptureTabStatus(Status.NATIVE_SERVICE_NOT_INITIALIZED);
        }

        if (tab.getWebContents() == null) {
            processCaptureTabStatus(Status.WEB_CONTENTS_GONE);
        }

        LongScreenshotsTabServiceJni.get().captureTabAndroid(mNativeLongScreenshotsTabService,
                tab.getId(), tab.getWebContents(), clipRect.left, clipRect.top, clipRect.right,
                clipRect.height());
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

    @NativeMethods
    interface Natives {
        void captureTabAndroid(long nativeLongScreenshotsTabService, int tabId,
                WebContents webContents, int clipX, int clipY, int clipWidth, int clipHeight);

        void longScreenshotsClosedAndroid(long nativeLongScreenshotsTabService);
    }
}
