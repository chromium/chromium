// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import org.chromium.android_webview.common.SafeModeAction;

/** An interface for defining a precaution WebView may take during SafeMode in non-embedded WebView. */
public interface NonEmbeddedSafeModeAction extends SafeModeAction {
    /**
     * Executes the given action. Implementations of this method should be Java-only (no JNI/C++)
     * because the native library may not yet be loaded. The return status is used for logging
     * purposes only.
     *
     * @return {@code true} if the action succeeded, {@code false} otherwise.
     */
    @Override
    public default boolean execute() {
        return true;
    }

    /**
     * Performs any setup/cleanup work required by the SafeModeAction on activation.
     * Not to be confused with the {@link #execute} method, which performs the primary work
     * of each safemode action.
     * <p>
     * The return status is used for logging purposes only.
     *
     * @return {@code true} if the action succeeded, {@code false} otherwise.
     */
    public default boolean onActivate() {
        return true;
    }

    /**
     * Performs any setup/cleanup work required by the SafeModeAction on deactivation.
     * Not to be confused with the {@link #execute} method, which performs the primary work
     * of each safemode action.
     * <p>
     * The return status is used for logging purposes only.
     *
     * @return {@code true} if the action succeeded, {@code false} otherwise.
     */
    public default boolean onDeactivate() {
        return true;
    }
}
