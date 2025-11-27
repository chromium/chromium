// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.embedder_support.util.PasswordEchoSettingObserver;
import org.chromium.components.embedder_support.util.PasswordEchoSettingState;

/**
 * Password echo setting controller for Webview.
 *
 * <p>Password echoing is the process of making the last character typed in a password field
 * momentarily visible before it is replaced by a mask (dot or asterisk).
 *
 * <p>This class syncs Android's password echo setting(s) with Webview's setting(s).
 */
@NullMarked
public class AwPasswordEchoSettingController implements PasswordEchoSettingObserver {
    private final AwSettings mSettings;
    private final PasswordEchoSettingState mState;

    public AwPasswordEchoSettingController(AwSettings settings) {
        mSettings = settings;
        mState = PasswordEchoSettingState.getInstance();
    }

    public void onAttachedToWindow() {
        mState.registerObserver(this);
        updatePasswordEchoState();
    }

    public void onDetachedFromWindow() {
        mState.unregisterObserver(this);
    }

    private void updatePasswordEchoState() {
        mSettings.setPasswordEchoEnabled(
                mState.getPasswordEchoEnabledPhysical(), mState.getPasswordEchoEnabledTouch());
    }

    @Override
    public void onSettingChange() {
        updatePasswordEchoState();
    }
}
