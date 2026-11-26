// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles TTC's keyboard shortcuts in one window, as saved on the TTC settings page: the keyboard
 * shortcut, which starts or ends a session, and the hold to speak shortcut, which TTC listens to
 * while it is held down. The shortcuts work wherever a keyboard is connected, on every form factor.
 */
@NullMarked
@JNINamespace("ttc")
class TtcShortcutHandler {
    /** Receives the shortcuts' presses and releases. */
    interface Delegate {
        /** The keyboard shortcut was pressed. */
        void onKeyboardShortcut();

        /** The hold to speak shortcut was pressed down. */
        void onHoldToSpeakPressed();

        /** The hold to speak shortcut was released, or stopped being the shortcut. */
        void onHoldToSpeakReleased();
    }

    private final Delegate mDelegate;
    private long mNativeTtcShortcutHandlerAndroid;

    /**
     * @param windowAndroid The window the shortcuts work in.
     * @param profile The profile whose TTC settings hold the shortcuts.
     * @param delegate Receives the shortcuts' presses and releases.
     */
    TtcShortcutHandler(WindowAndroid windowAndroid, Profile profile, Delegate delegate) {
        mDelegate = delegate;
        mNativeTtcShortcutHandlerAndroid =
                TtcShortcutHandlerJni.get().init(this, windowAndroid, profile);
    }

    /** Stops handling the shortcuts. */
    void destroy() {
        if (mNativeTtcShortcutHandlerAndroid == 0) return;
        TtcShortcutHandlerJni.get().destroy(mNativeTtcShortcutHandlerAndroid);
        mNativeTtcShortcutHandlerAndroid = 0;
    }

    @CalledByNative
    private void onKeyboardShortcut() {
        mDelegate.onKeyboardShortcut();
    }

    @CalledByNative
    private void onHoldToSpeakPressed() {
        mDelegate.onHoldToSpeakPressed();
    }

    @CalledByNative
    private void onHoldToSpeakReleased() {
        mDelegate.onHoldToSpeakReleased();
    }

    @NativeMethods
    interface Natives {
        long init(
                TtcShortcutHandler self,
                @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
                @JniType("Profile*") Profile profile);

        void destroy(long nativeTtcShortcutHandlerAndroid);
    }
}
