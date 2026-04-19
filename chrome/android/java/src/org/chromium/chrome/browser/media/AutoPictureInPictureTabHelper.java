// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static android.view.Display.INVALID_DISPLAY;

import android.graphics.Rect;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * A tab helper that stores the state of Auto-PiP permissions for a specific WebContents. This
 * includes whether the "Allow Once" permission has been granted, if Auto-PiP has been triggered
 * recently, and manages the active permission controller instance.
 *
 * <p>The lifecycle of this object is tied to the WebContents via the UserData pattern. It is
 * created lazily when needed and destroyed automatically when the WebContents is destroyed.
 */
@NullMarked
public class AutoPictureInPictureTabHelper implements UserData {
    public static final Class<AutoPictureInPictureTabHelper> USER_DATA_KEY =
            AutoPictureInPictureTabHelper.class;

    /** The WebContents this helper is attached to. */
    private final WebContents mWebContents;

    /**
     * Observes {@link WebContents} events to clear the {@link #mHasAllowOnce} state when the
     * primary page changes.
     */
    private final WebContentsObserver mWebContentsObserver;

    private boolean mHasAllowOnce;
    private @Nullable AutoPictureInPicturePermissionController mPermissionController;

    /**
     * Retrieves the {@link AutoPictureInPictureTabHelper} for the given {@link WebContents},
     * creating it if it doesn't already exist. The return value can be null if the {@link
     * WebContents} is not fully initialized or its internal data storage has been
     * garbage-collected.
     *
     * @param webContents The WebContents to get the helper for.
     * @return The {@link AutoPictureInPictureTabHelper} or null if it cannot be created or
     *     retrieved.
     */
    public static @Nullable AutoPictureInPictureTabHelper fromWebContents(WebContents webContents) {
        return webContents.getOrSetUserData(USER_DATA_KEY, AutoPictureInPictureTabHelper::new);
    }

    /**
     * Retrieves the {@link AutoPictureInPictureTabHelper} for the given {@link WebContents} if it
     * exists.
     *
     * @param webContents The WebContents to get the helper for.
     * @return The {@link AutoPictureInPictureTabHelper} or null if it does not exist.
     */
    public static @Nullable AutoPictureInPictureTabHelper getIfPresent(WebContents webContents) {
        return webContents.getOrSetUserData(USER_DATA_KEY, null);
    }

    public AutoPictureInPictureTabHelper(WebContents webContents) {
        mWebContents = webContents;
        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void primaryPageChanged(Page page) {
                        setHasAllowOnce(false);
                    }
                };
    }

    /** Sets whether the user has granted "Allow Once" permission for this session. */
    public void setHasAllowOnce(boolean hasAllowOnce) {
        mHasAllowOnce = hasAllowOnce;
    }

    /** Returns true if the user has granted "Allow Once" permission for this session. */
    public boolean hasAllowOnce() {
        return mHasAllowOnce;
    }

    /**
     * Sets the active permission controller.
     *
     * @param controller The active controller, or null if the prompt is dismissed.
     */
    public void setPermissionController(
            @Nullable AutoPictureInPicturePermissionController controller) {
        if (mPermissionController == controller) {
            return;
        }

        AutoPictureInPicturePermissionController oldController = mPermissionController;
        mPermissionController = controller;

        // Ensure the old one is dismissed
        if (oldController != null) {
            oldController.dismiss();
        }
    }

    /** Returns the active permission controller, or null if none is showing. */
    public @Nullable AutoPictureInPicturePermissionController getPermissionController() {
        return mPermissionController;
    }

    /**
     * Returns the cached window bounds from the native C++ cache.
     *
     * @param requestedBounds The bounds currently requested by the site.
     * @return The cached user-resized window bounds in pixels, or null if there is no match.
     */
    public @Nullable Rect getCachedWindowBounds(@Nullable Rect requestedBounds) {
        int openerDisplayId = INVALID_DISPLAY;
        float dipScale = 1.0f;
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window != null) {
            openerDisplayId = window.getDisplay().getDisplayId();
            dipScale = window.getDisplay().getDipScale();
        }

        // Use a negative value to indicate that the site did not request a specific size.
        // The C++ bridge will convert this to std::nullopt by checking if dimensions are > 0.
        // Convert pixels to DIPs to match C++ cache expectations.
        int requestedWidth =
                requestedBounds != null ? (int) (requestedBounds.width() / dipScale) : -1;
        int requestedHeight =
                requestedBounds != null ? (int) (requestedBounds.height() / dipScale) : -1;

        return PictureInPictureBoundsCacheBridge.getBoundsForNewWindow(
                mWebContents, openerDisplayId, requestedWidth, requestedHeight);
    }

    @Override
    public void destroy() {
        mWebContentsObserver.observe(null);
    }
}
