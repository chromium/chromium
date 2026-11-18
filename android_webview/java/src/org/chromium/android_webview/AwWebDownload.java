// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.MediaStore;

import androidx.annotation.UiThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A download started in a WebView that has a download handler set.
 *
 * <p>WebView keeps receiving the response body into a temporary file until the embedder either
 * calls {@link #saveTo} to write the download to a destination of its choice, or {@link #cancel()}
 * to reject it. If the embedder drops this object without calling either, the download is cancelled
 * when the object is garbage collected. Once {@link #saveTo} has been called, WebView keeps the
 * download alive until the outcome has been reported.
 *
 * <p>{@link #saveTo} and {@link #cancel()} must be called on the UI thread. The getters can be
 * called from any thread.
 */
@JNINamespace("android_webview")
@Lifetime.Temporary
@NullMarked
public final class AwWebDownload {
    @JniType("::android_webview::AwWebDownload")
    interface NativeAwWebDownload extends JniTypeToken {}

    /** Receives the outcome of {@link #saveTo}. Called on the UI thread. */
    public interface SaveCallback {
        void onSuccess();

        void onError(@AwWebDownloadResult int errorCode);
    }

    private final String mUrl;
    private final String mMimeType;
    private final long mContentLength;
    private final String mSuggestedFilename;

    // Only accessed on the UI thread.
    private @Nullable JniUniquePtr<NativeAwWebDownload> mNativeAwWebDownload;
    // Responsible for deleting the native peer.
    private @Nullable CleanupReference mCleanupReference;
    private boolean mSaveRequested;
    private boolean mCancelled;
    private @Nullable SaveCallback mSaveCallback;

    AwWebDownload(
            JniUniquePtr<NativeAwWebDownload> nativeAwWebDownload,
            String url,
            String mimeType,
            long contentLength,
            String suggestedFilename) {
        mUrl = url;
        mMimeType = mimeType;
        mContentLength = contentLength;
        mSuggestedFilename = suggestedFilename;
        mNativeAwWebDownload = nativeAwWebDownload;
        mCleanupReference = new CleanupReference(this, (unused) -> nativeAwWebDownload.destroy());
    }

    /** Returns the URL of the download (after redirects). */
    public String getUrl() {
        return mUrl;
    }

    /** Returns the MIME type of the download, which may be empty. */
    public String getMimeType() {
        return mMimeType;
    }

    /** Returns the expected size of the response body in bytes, or -1 if unknown. */
    public long getContentLength() {
        return mContentLength;
    }

    /** Returns the filename WebView would suggest for the download. */
    public String getSuggestedFilename() {
        return mSuggestedFilename;
    }

    /**
     * Writes the download to {@code destination}, which must be a {@code file://} Uri with an
     * absolute path or a {@code content://} MediaStore item. {@code callback} is called exactly
     * once on the UI thread, possibly before this method returns. If the download fails or is
     * cancelled, WebView deletes what it wrote to {@code destination}.
     *
     * @throws IllegalStateException if called more than once, or after {@link #cancel()}.
     * @throws IllegalArgumentException if {@code destination} is not supported.
     */
    @UiThread
    public void saveTo(Uri destination, SaveCallback callback) {
        ThreadUtils.checkUiThread();
        String target = toDownloadTarget(destination);
        if (mSaveRequested) {
            throw new IllegalStateException("saveTo() may only be called once.");
        }
        if (mCancelled) {
            throw new IllegalStateException("saveTo() may not be called after cancel().");
        }
        mSaveRequested = true;
        mSaveCallback = callback;
        AwWebDownloadJni.get().saveTo(assumeNonNull(mNativeAwWebDownload), this, target);
    }

    /**
     * Cancels the download, before or after {@link #saveTo}. If a {@link #saveTo} request is in
     * flight, its callback receives {@link AwWebDownloadResult#ERROR_CANCELLED}, possibly before
     * this method returns. Calling this more than once, or after the download finished, has no
     * effect. {@link #saveTo} may not be called afterwards.
     */
    @UiThread
    public void cancel() {
        ThreadUtils.checkUiThread();
        mCancelled = true;
        if (mNativeAwWebDownload == null) return;
        AwWebDownloadJni.get().cancel(mNativeAwWebDownload);
        // With a saveTo() request in flight, onFinished() destroys the native peer.
        if (!mSaveRequested) destroyNative();
    }

    @CalledByNative
    private void onFinished(
            @JniType("::android_webview::AwWebDownload::Result") @AwWebDownloadResult int result) {
        destroyNative();
        if (mSaveCallback == null) return;
        if (result == AwWebDownloadResult.SUCCESS) {
            mSaveCallback.onSuccess();
        } else {
            mSaveCallback.onError(result);
        }
        mSaveCallback = null;
    }

    private void destroyNative() {
        mNativeAwWebDownload = null;
        if (mCleanupReference != null) {
            mCleanupReference.cleanupNow();
            mCleanupReference = null;
        }
    }

    /** Converts a supported destination Uri into the path or URI string used by native code. */
    private static String toDownloadTarget(Uri destination) {
        Uri uri = destination.normalizeScheme();
        String scheme = uri.getScheme();
        // file:// Uri with an absolute path.
        if (ContentResolver.SCHEME_FILE.equals(scheme)) {
            String path = uri.getPath();
            if (path == null || !path.startsWith("/")) {
                throw new IllegalArgumentException(
                        "file:// destination must have an absolute path: " + destination);
            }
            return path;
        }
        // MediaStore item.
        if (ContentResolver.SCHEME_CONTENT.equals(scheme)
                && MediaStore.AUTHORITY.equals(uri.getAuthority())) {
            return uri.toString();
        }
        throw new IllegalArgumentException(
                "Unsupported download destination (expected a file:// Uri or a MediaStore item): "
                        + destination);
    }

    @NativeMethods
    interface Natives {
        void saveTo(
                JniPtr<NativeAwWebDownload> self,
                AwWebDownload javaDownload,
                @JniType("std::string") String target);

        void cancel(JniPtr<NativeAwWebDownload> self);
    }
}
