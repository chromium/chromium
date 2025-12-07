// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

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

    private boolean mHasAllowOnce;
    private boolean mAutoPipTriggered;
    private @Nullable AutoPictureInPicturePermissionController mPermissionController;

    public static AutoPictureInPictureTabHelper get(WebContents webContents) {
        AutoPictureInPictureTabHelper helper =
                webContents.getOrSetUserData(USER_DATA_KEY, AutoPictureInPictureTabHelper::new);
        // UserDataHost can theoretically return null, but our factory never does.
        // This assertion satisfies NullAway.
        assert helper != null;
        return helper;
    }

    public AutoPictureInPictureTabHelper(WebContents webContents) {}

    /** Sets whether the user has granted "Allow Once" permission for this session. */
    public void setHasAllowOnce(boolean hasAllowOnce) {
        mHasAllowOnce = hasAllowOnce;
    }

    /** Returns true if the user has granted "Allow Once" permission for this session. */
    public boolean hasAllowOnce() {
        return mHasAllowOnce;
    }

    /** Sets whether Auto-PiP has been triggered recently. */
    public void setAutoPipTriggered(boolean triggered) {
        mAutoPipTriggered = triggered;
    }

    /** Returns true if Auto-PiP has been triggered recently. */
    public boolean isAutoPipTriggered() {
        return mAutoPipTriggered;
    }

    /** Clears the auto-pip triggered flag without consuming it (used for navigation resets). */
    public void clearAutoPipTriggered() {
        mAutoPipTriggered = false;
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
}
