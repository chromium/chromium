// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import android.content.Intent;
import android.net.Uri;

import androidx.annotation.AnyThread;
import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * Manages content URIs for screenshots. It handles URI generation, reusability, and secure
 * permission management.
 *
 * <p>Thread safety: Methods in this class are annotated with {@link MainThread} or {@link
 * AnyThread} to indicate their thread expectations. Shared state is guarded by an internal lock,
 * making the {@link AnyThread} methods safe to call from multiple threads.
 */
@NullMarked
public class ScreenshotUriProvider {
    private static final String TAG = "ScreenshotUriProv";
    private static final long INVALIDATE_URI_DELAY_MS = TimeUnit.MINUTES.toMillis(10);

    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static @Nullable ScreenshotInvocationState sInvocationState;

    /**
     * Returns a generated or reused content URI for a screenshot of the currently active page, or
     * null if generation fails. This method must be called on the UI thread because it captures Tab
     * metadata.
     *
     * @param tabSupplier The supplier used to access the current tab and check its state.
     * @param targetPackage The package name to grant READ permission to.
     */
    @MainThread
    public static @Nullable Uri getScreenshotUriForCurrentTab(
            Supplier<@Nullable Tab> tabSupplier, String targetPackage) {
        ScreenshotInvocationState newInvocationState =
                ScreenshotInvocationState.create(tabSupplier);

        if (newInvocationState == null) {
            Log.w(TAG, "getScreenshotUriForCurrentTab - invalid invocation state.");
            return null;
        }

        synchronized (sLock) {
            if (sInvocationState != null && newInvocationState.canReuse(sInvocationState)) {
                return sInvocationState.getContentUri();
            }

            return createNewStateAndGrantAccess(newInvocationState, targetPackage);
        }
    }

    /**
     * Returns the state associated with a given invocation ID, or null if not found or expired. It
     * is used by the ContentProvider to fulfill queries for previously generated URIs. This method
     * is thread-safe and can be called from any thread.
     *
     * @param invocationId The unique ID of the invocation.
     */
    @AnyThread
    public static @Nullable ScreenshotInvocationState getInvocationState(String invocationId) {
        synchronized (sLock) {
            if (sInvocationState != null) {
                String currentId = sInvocationState.getInvocationId();
                if (currentId != null && currentId.equals(invocationId)) {
                    return sInvocationState;
                }
            }
        }
        Log.d(TAG, "No matching or active invocation state found for ID: %s", invocationId);
        return null;
    }

    /**
     * Clears cached state if it matches the provided ID, or clears everything if the ID is null.
     * This method is thread-safe and can be called from any thread.
     *
     * @param invocationId The ID to clear, or null to clear the current state.
     */
    @AnyThread
    public static void clearCachedContent(@Nullable String invocationId) {
        synchronized (sLock) {
            clearCachedContentInternal(invocationId);
        }
    }

    @GuardedBy("sLock")
    private static @Nullable Uri createNewStateAndGrantAccess(
            ScreenshotInvocationState newInvocationState, String targetPackage) {
        // Clear anything currently cached.
        clearCachedContentInternal(null);

        // Initialize the state and generate the content URI.
        newInvocationState.initState();
        sInvocationState = newInvocationState;

        Uri contentUri = sInvocationState.getContentUri();
        String invocationId = sInvocationState.getInvocationId();

        if (contentUri == null || invocationId == null) {
            Log.e(TAG, "Failed to initialize screenshot state.");
            sInvocationState = null;
            return null;
        }

        grantReadAccess(contentUri, targetPackage);

        // Schedule automatic invalidation.
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> clearCachedContent(invocationId),
                INVALIDATE_URI_DELAY_MS);

        return contentUri;
    }

    private static void grantReadAccess(Uri uri, String packageName) {
        Log.d(TAG, "Granting URI permission to package: %s", packageName);
        ContextUtils.getApplicationContext()
                .grantUriPermission(packageName, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
    }

    private static void revokeReadAccess(Uri uri) {
        Log.d(TAG, "Revoking URI permission: %s", uri);
        ContextUtils.getApplicationContext()
                .revokeUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
    }

    @GuardedBy("sLock")
    private static void clearCachedContentInternal(@Nullable String invocationId) {
        if (sInvocationState == null) return;

        String currentId = sInvocationState.getInvocationId();
        // If an ID is provided, ONLY clear if it matches.
        if (invocationId != null && (currentId == null || !currentId.equals(invocationId))) {
            return;
        }

        Log.d(TAG, "Clearing cached screenshot state for ID: %s", currentId);
        Uri contentUri = sInvocationState.getContentUri();
        if (contentUri != null) {
            revokeReadAccess(contentUri);
        }
        sInvocationState = null;
    }
}
