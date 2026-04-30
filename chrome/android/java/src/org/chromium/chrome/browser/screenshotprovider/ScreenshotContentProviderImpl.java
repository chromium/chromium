// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import android.app.Activity;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.ParcelFileDescriptor.AutoCloseOutputStream;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;
import org.chromium.chrome.browser.feedback.ScreenshotMode;
import org.chromium.chrome.browser.feedback.ScreenshotSource;
import org.chromium.chrome.browser.feedback.ScreenshotTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.io.BufferedOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import org.chromium.base.TraceEvent;

/** ContentProvider that serves screenshots. */
@UsedByReflection("ScreenshotContentProvider.java")
@NullMarked
public class ScreenshotContentProviderImpl extends SplitCompatContentProvider.Impl {
    private static final String TAG = "ScreenshotContentPro";
    private static final String CONTENT_TYPE = "image/png";
    private static final int CAPTURE_TIMEOUT_MS = 5000;

    @Override
    public @Nullable Cursor query(
            Uri uri,
            String @Nullable [] strings,
            @Nullable String s,
            String @Nullable [] strings1,
            @Nullable String s1) {
        return null;
    }

    @Override
    public @Nullable String getType(Uri uri) {
        return CONTENT_TYPE;
    }

    @Override
    public @Nullable Uri insert(Uri uri, @Nullable ContentValues contentValues) {
        return null;
    }

    @Override
    public int delete(Uri uri, @Nullable String s, String @Nullable [] strings) {
        return 0;
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues contentValues,
            @Nullable String s,
            String @Nullable [] strings) {
        return 0;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        ThreadUtils.assertOnBackgroundThread();

        validateReadMode(mode);
        String invocationId = extractInvocationId(uri);
        ScreenshotInvocationState state = getValidatedState(invocationId);

        CompletableFuture<Bitmap> captureFuture = getViewportScreenshot(state);

        return waitForCaptureAndCreatePfd(captureFuture);
    }

    protected ScreenshotSource createScreenshotSource(Activity activity) {
        return new ScreenshotTask(activity, ScreenshotMode.COMPOSITOR);
    }

    private Bitmap blockOnCaptureFuture(CompletableFuture<Bitmap> captureFuture)
            throws FileNotFoundException {
        try {
            return captureFuture.get(CAPTURE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | ExecutionException e) {
            Log.e(TAG, "Error capturing screenshot", e);
            throw new FileNotFoundException("Error capturing screenshot: " + e.getMessage());
        } catch (TimeoutException e) {
            Log.e(TAG, "Timeout capturing screenshot", e);
            throw new FileNotFoundException("Timeout capturing screenshot");
        }
    }

    private ParcelFileDescriptor waitForCaptureAndCreatePfd(CompletableFuture<Bitmap> captureFuture)
            throws FileNotFoundException {
        try (var t =
                TraceEvent.scoped("ScreenshotContentProviderImpl.waitForCaptureAndCreatePfd")) {
            ThreadUtils.assertOnBackgroundThread();
            try {
                // Here we are blocking the background thread, and waiting for Ui thread to return
                // the
                // bitmap.
                Bitmap bitmap = blockOnCaptureFuture(captureFuture);
                if (bitmap == null) {
                    throw new FileNotFoundException("Failed to capture screenshot: no bitmap");
                }
                return createOutputFileDescriptorForBitmap(bitmap);
            } catch (IOException e) {
                Log.e(TAG, "Error creating file descriptor", e.getMessage());
                throw new FileNotFoundException("IO error: " + e.getMessage());
            }
        }
    }

    private void validateReadMode(String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) {
            Log.e(TAG, "Unsupported mode: %s. Only 'r' is supported.", mode);
            throw new FileNotFoundException("Only read mode is supported");
        }
    }

    private String extractInvocationId(Uri uri) throws FileNotFoundException {
        String invocationId = uri.getLastPathSegment();
        if (invocationId == null) {
            Log.e(TAG, "No invocation ID found in URI: %s", uri);
            throw new FileNotFoundException("Invalid URI: no invocation ID");
        }
        return invocationId;
    }

    private ScreenshotInvocationState getValidatedState(String invocationId)
            throws FileNotFoundException {
        ScreenshotInvocationState state = ScreenshotUriProvider.getInvocationState(invocationId);
        if (state == null) {
            Log.e(TAG, "Invalid or expired invocation ID: %s", invocationId);
            throw new FileNotFoundException("Invalid or expired invocation ID");
        }
        return state;
    }

    /**
     * Creates a {@link ParcelFileDescriptor} to stream the provided bitmap.
     *
     * @param bitmap The bitmap to be streamed.
     * @return A read-side PFD.
     * @throws IOException If the pipe cannot be created.
     */
    private ParcelFileDescriptor createOutputFileDescriptorForBitmap(Bitmap bitmap)
            throws IOException {
        // This method is called from openFile(), which is called on a background thread.
        ThreadUtils.assertOnBackgroundThread();
        ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createReliablePipe();
        ParcelFileDescriptor readSide = pipe[0];
        ParcelFileDescriptor writeSide = pipe[1];

        // Post a task to write bytes after returning readSide, otherwise the write loop gets stuck
        // because there's nothing reading from it.
        PostTask.postTask(
                TaskTraits.USER_VISIBLE,
                () -> {
                    try (AutoCloseOutputStream autoCloseOutputStream =
                                    new AutoCloseOutputStream(writeSide);
                            BufferedOutputStream outputStream =
                                    new BufferedOutputStream(autoCloseOutputStream)) {
                        if (!bitmap.compress(Bitmap.CompressFormat.PNG, 100, outputStream)) {
                            Log.e(TAG, "Failed to compress bitmap to PNG");
                            writeSide.closeWithError("Failed to compress bitmap");
                        }
                        outputStream.flush();
                    } catch (IOException e) {
                        Log.e(TAG, "Error writing to pipe", e);
                        try {
                            writeSide.closeWithError(
                                    "IOException on write side: " + e.getMessage());
                        } catch (IOException ignored) {
                        }
                    } finally {
                        bitmap.recycle();
                    }
                });
        return readSide;
    }

    private CompletableFuture<Bitmap> getViewportScreenshot(ScreenshotInvocationState state) {
        CompletableFuture<Bitmap> future = new CompletableFuture<>();
        ThreadUtils.runOnUiThread(() -> setViewportScreenshotFuture(state, future));
        return future;
    }

    private void setViewportScreenshotFuture(
            ScreenshotInvocationState state, CompletableFuture<Bitmap> future) {
        try (var t =
                TraceEvent.scoped("ScreenshotContentProviderImpl.setViewportScreenshotFuture")) {
            ThreadUtils.assertOnUiThread();
            Activity activity = getValidActivityFromState(state);
            if (activity == null) {
                Log.e(TAG, "Failed to get valid activity");
                future.completeExceptionally(new Exception("Failed to get valid activity"));
                return;
            }

            Log.d(TAG, "Starting screenshot capture for invocation: %s", state.getInvocationId());
            ScreenshotSource task = createScreenshotSource(activity);
            task.capture(() -> setBitmapFuture(task.getScreenshot(), future));
        }
    }

    private @Nullable Activity getValidActivityFromState(ScreenshotInvocationState state) {
        // Accessing Tab and Context must be done on the main thread.
        ThreadUtils.assertOnUiThread();
        Log.d(TAG, "Getting valid activity from state");

        Tab tab = state.getTabSupplier().get();
        if (tab == null) {
            Log.e(TAG, "Tab is null");
            return null;
        }
        if (tab.isDestroyed()) {
            Log.e(TAG, "Tab is destroyed");
            return null;
        }

        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) {
            Log.e(TAG, "WindowAndroid is null");
            return null;
        }

        GURL url = tab.getUrl();
        if (url == null) {
            Log.e(TAG, "URL is null");
            return null;
        }

        if (!state.getInvokedUrl().equals(url.getSpec())) {
            Log.e(TAG, "URL mismatch: expected %s, got %s", state.getInvokedUrl(), url.getSpec());
            return null;
        }

        Context context = tab.getContext();
        if (context == null) {
            Log.e(TAG, "Context is null");
            return null;
        }

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            Log.e(TAG, "Activity is null");
        }
        return activity;
    }

    private static void setBitmapFuture(@Nullable Bitmap bitmap, CompletableFuture<Bitmap> future) {
        if (bitmap == null) {
            Log.e(TAG, "Failed to capture screenshot");
            future.completeExceptionally(new Exception("Failed to capture screenshot"));
            return;
        }
        future.complete(bitmap);
    }
}
