// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * State related to a screenshot invocation. It stores metadata about the page when a screenshot URI
 * was requested.
 */
@NullMarked
public class ScreenshotInvocationState {
    private static final String AUTHORITY_SUFFIX = ".ScreenshotContentProvider";
    private static final long REUSE_URI_MAX_AGE_MS = TimeUnit.MINUTES.toMillis(1);
    private static final int SCROLL_DIFF_THRESHOLD = 20; // 20 pixels

    private final String mInvokedUrl;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final long mTimestamp;
    private final int mScrollX;
    private final int mScrollY;

    private @Nullable String mInvocationId;
    private @Nullable Uri mContentUri;

    /**
     * Returns a new invocation state capturing the current tab metadata, or null if tab metadata
     * cannot be retrieved. This method must be called on the UI thread.
     *
     * @param tabSupplier The supplier for the current tab. This is used to check if the tab is
     *     still active, before capturing the screenshot.
     */
    @MainThread
    public static @Nullable ScreenshotInvocationState create(Supplier<@Nullable Tab> tabSupplier) {
        Tab tab = tabSupplier.get();
        if (tab == null || tab.getWebContents() == null || tab.getUrl() == null) {
            return null;
        }
        return new ScreenshotInvocationState(tab, tabSupplier);
    }

    private ScreenshotInvocationState(Tab tab, Supplier<@Nullable Tab> tabSupplier) {
        mInvokedUrl = tab.getUrl().getSpec();
        mTabSupplier = tabSupplier;
        mTimestamp = SystemClock.elapsedRealtime();

        WebContents webContents = tab.getWebContents();
        RenderCoordinates coords =
                webContents != null ? RenderCoordinates.fromWebContents(webContents) : null;
        mScrollX = coords != null ? coords.getScrollXPixInt() : 0;
        mScrollY = coords != null ? coords.getScrollYPixInt() : 0;
    }

    /**
     * Initializes the state with a unique ID and content URI if not already done. This is typically
     * called on the UI thread during creation.
     */
    @MainThread
    public void initState() {
        if (mInvocationId == null) {
            mInvocationId = UUID.randomUUID().toString();
            mContentUri =
                    new Uri.Builder()
                            .scheme(ContentResolver.SCHEME_CONTENT)
                            .authority(
                                    ContextUtils.getApplicationContext().getPackageName()
                                            + AUTHORITY_SUFFIX)
                            .appendPath(mInvocationId)
                            .build();
        }
    }

    /** Returns the unique ID for this invocation. This method is thread-safe. */
    @AnyThread
    public @Nullable String getInvocationId() {
        assert mInvocationId != null : "Invocation ID not initialized";
        return mInvocationId;
    }

    /** Returns the URL for which the screenshot was invoked. This method is thread-safe. */
    @AnyThread
    public String getInvokedUrl() {
        return mInvokedUrl;
    }

    /** Returns the supplier for the tab associated with this state. This method is thread-safe. */
    @AnyThread
    public Supplier<@Nullable Tab> getTabSupplier() {
        return mTabSupplier;
    }

    /** Returns the content URI associated with this invocation. This method is thread-safe. */
    @AnyThread
    public @Nullable Uri getContentUri() {
        assert mContentUri != null : "Content URI not initialized";
        return mContentUri;
    }

    /**
     * Returns whether this new state can reuse a previously cached state. This method is
     * thread-safe.
     *
     * @param oldState The previously cached state.
     */
    @AnyThread
    public boolean canReuse(@Nullable ScreenshotInvocationState oldState) {
        if (oldState == null) return false;
        if (!mInvokedUrl.equals(oldState.mInvokedUrl)) return false;

        // Check if the old state is too stale.
        if (mTimestamp - oldState.mTimestamp > REUSE_URI_MAX_AGE_MS) {
            return false;
        }

        // Check if the user has scrolled significantly compared to the old state.
        return Math.abs(mScrollX - oldState.mScrollX) <= SCROLL_DIFF_THRESHOLD
                && Math.abs(mScrollY - oldState.mScrollY) <= SCROLL_DIFF_THRESHOLD;
    }
}
