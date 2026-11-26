// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import android.view.KeyEvent;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Helpers for TTC's keyboard shortcuts. The shortcut prefs hold ui::Command strings, such as
 * "Alt+Shift+K", so that every platform can read them, and ui::Accelerator does the work, as it
 * does for Chrome's own shortcuts.
 */
@NullMarked
@JNINamespace("ttc")
final class TtcShortcutUtils {
    private TtcShortcutUtils() {}

    /**
     * Returns the shortcut pref value that names a key press, or "" if none does, e.g. because the
     * key is a modifier or isn't supported.
     */
    static String fromKeyEvent(KeyEvent event) {
        return TtcShortcutUtilsJni.get().fromKeyEvent(event);
    }

    /** Returns whether {@code shortcut}, a shortcut pref value, can be used as a TTC shortcut. */
    static @TtcShortcutStatus int getStatus(String shortcut) {
        return TtcShortcutUtilsJni.get().getStatus(shortcut);
    }

    /**
     * Returns the value that {@link #fromKeyEvent} gives for the shortcut that {@code shortcut}, a
     * shortcut pref value, names, or "" if it isn't a valid shortcut. Different values can name the
     * same shortcut, e.g. with the modifiers in another order.
     */
    static String canonicalize(String shortcut) {
        return TtcShortcutUtilsJni.get().canonicalize(shortcut);
    }

    /**
     * Returns the labels of the keys of {@code shortcut}, a shortcut pref value, modifiers first,
     * in the user's language. Empty if it doesn't name a shortcut.
     */
    static String[] getKeyLabels(String shortcut) {
        return TtcShortcutUtilsJni.get().getKeyLabels(shortcut);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String fromKeyEvent(KeyEvent event);

        @TtcShortcutStatus
        int getStatus(@JniType("std::string") String shortcut);

        @JniType("std::string")
        String canonicalize(@JniType("std::string") String shortcut);

        @JniType("std::vector<std::u16string>")
        String[] getKeyLabels(@JniType("std::string") String shortcut);
    }
}
