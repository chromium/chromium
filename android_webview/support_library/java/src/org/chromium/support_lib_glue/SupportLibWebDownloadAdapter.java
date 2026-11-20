// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.net.Uri;

import org.chromium.android_webview.AwWebDownload;
import org.chromium.android_webview.AwWebDownloadResult;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.WebDownloadBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.concurrent.Executor;
import java.util.function.Consumer;

/** Adapter between WebDownloadBoundaryInterface and AwWebDownload. */
@Lifetime.Temporary
@NullMarked
class SupportLibWebDownloadAdapter implements WebDownloadBoundaryInterface {
    private final AwWebDownload mDownload;

    SupportLibWebDownloadAdapter(AwWebDownload download) {
        mDownload = download;
    }

    @Override
    public String getUrl() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_GET_URL")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_GET_URL);
            return mDownload.getUrl();
        }
    }

    @Override
    public String getMimeType() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_GET_MIME_TYPE")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_GET_MIME_TYPE);
            return mDownload.getMimeType();
        }
    }

    @Override
    public long getContentLength() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_GET_CONTENT_LENGTH")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_GET_CONTENT_LENGTH);
            return mDownload.getContentLength();
        }
    }

    @Override
    public String getSuggestedFilename() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_GET_SUGGESTED_FILENAME")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_GET_SUGGESTED_FILENAME);
            return mDownload.getSuggestedFilename();
        }
    }

    @Override
    public void saveTo(
            Uri destination,
            Executor executor,
            Consumer<Long> resultCallback,
            Consumer<@ErrorCode Integer> errorCallback) {
        // Fail now rather than once the download is over.
        if (executor == null || resultCallback == null || errorCallback == null) {
            throw new NullPointerException("Executor and callbacks must not be null");
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_SAVE_TO")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_SAVE_TO);
            mDownload.saveTo(
                    destination,
                    new AwWebDownload.SaveCallback() {
                        @Override
                        public void onSuccess(long totalBytes) {
                            executor.execute(() -> callResultCallback(resultCallback, totalBytes));
                        }

                        @Override
                        public void onError(@AwWebDownloadResult int errorCode) {
                            @ErrorCode int error = toErrorCode(errorCode);
                            executor.execute(() -> callErrorCallback(errorCallback, error));
                        }
                    });
        }
    }

    @Override
    public void cancel() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.WEB_DOWNLOAD_CANCEL")) {
            recordApiCall(ApiCall.WEB_DOWNLOAD_CANCEL);
            mDownload.cancel();
        }
    }

    private static void callResultCallback(Consumer<Long> callback, long totalBytes) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.AndroidX.WEB_DOWNLOAD_SAVE_TO_ON_RESULT")) {
            callback.accept(totalBytes);
        }
    }

    private static void callErrorCallback(
            Consumer<@ErrorCode Integer> callback, @ErrorCode int error) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.AndroidX.WEB_DOWNLOAD_SAVE_TO_ON_ERROR")) {
            callback.accept(error);
        }
    }

    private static @ErrorCode int toErrorCode(@AwWebDownloadResult int errorCode) {
        return switch (errorCode) {
            case AwWebDownloadResult.ERROR_CANCELLED -> ErrorCode.CANCELLED;
            case AwWebDownloadResult.ERROR_NETWORK -> ErrorCode.NETWORK;
            case AwWebDownloadResult.ERROR_FILE -> ErrorCode.FILE;
            case AwWebDownloadResult.ERROR_INSUFFICIENT_SPACE -> ErrorCode.INSUFFICIENT_SPACE;
            case AwWebDownloadResult.ERROR_SERVER -> ErrorCode.SERVER;
            case AwWebDownloadResult.ERROR_SERVER_ACCESS_DENIED -> ErrorCode.SERVER_ACCESS_DENIED;
            // AwWebDownloadResult.ERROR_UNKNOWN, and any error without a matching ErrorCode.
            default -> ErrorCode.UNKNOWN;
        };
    }
}
