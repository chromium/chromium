// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;

/**
 * Helper for gluing interactions with the Play store's AppUpdateManager with Chrome.  This
 * involves hooking up to Play as a listener for install state changes, should only happen if we are
 * in the foreground.
 */
public interface InlineUpdateController {
    /**
     * Enables or disables the controller.
     * @param enabled true iff the controller should be enabled.
     */
    void setEnabled(boolean enabled);

    /**
     * @return The current state of the inline update process.  May be {@code null} if the state
     * hasn't been determined yet.
     */
    @Nullable
    @UpdateStatusProvider.UpdateState
    Integer getStatus();

    /**
     * Starts the update, if possible.  This will send an {@link Intent} out to play, which may
     * cause Chrome to move to the background.
     * @param activity The {@link Activity} to use to interact with Play.
     */
    void startUpdate(Activity activity);

    /**
     * Completes the Play installation process, if possible.  This may cause Chrome to restart.
     */
    void completeUpdate();
}
