// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shortcuts;

import android.view.KeyEvent;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.AcceleratorManager;
import org.chromium.ui.base.KeyEventPreHandleResult;
import org.chromium.ui.base.WindowAndroid;

/**
 * Registers browser keyboard shortcuts on a window's {@link AcceleratorManager} and routes matched
 * commands through {@link BrowserCommandDelegate}.
 */
@NullMarked
public class BrowserShortcutManager {
    private final AcceleratorManager mAcceleratorManager;
    private final @Nullable LifetimeAssert mLifetimeAssert;
    private long mNativeBrowserShortcutManagerAndroid;

    /**
     * Creates a shortcut manager and registers browser accelerators on {@code windowAndroid}.
     *
     * <p>The caller must call {@link #destroy()} before destroying the window's {@link
     * AcceleratorManager}.
     *
     * @param windowAndroid The window whose {@link AcceleratorManager} receives shortcut targets.
     * @param commandDelegate The delegate that evaluates and executes matched browser commands.
     */
    public BrowserShortcutManager(
            WindowAndroid windowAndroid, BrowserCommandDelegate commandDelegate) {
        mAcceleratorManager = AcceleratorManager.getOrCreate(windowAndroid);
        mNativeBrowserShortcutManagerAndroid =
                BrowserShortcutManagerJni.get().init(windowAndroid, commandDelegate);
        mLifetimeAssert = LifetimeAssert.create(this);
    }

    /**
     * Evaluates {@code event} prior to renderer dispatch, executing reserved browser shortcuts and
     * non-browser window accelerators immediately while flagging non-reserved key-down browser
     * shortcuts with {@link KeyEventPreHandleResult#NOT_HANDLED_IS_SHORTCUT}.
     *
     * @param event The incoming key event.
     * @return The {@link KeyEventPreHandleResult} describing whether {@code event} was handled or
     *     recognized as a browser shortcut.
     */
    public @KeyEventPreHandleResult int preHandleKeyEvent(KeyEvent event) {
        assert mNativeBrowserShortcutManagerAndroid != 0;
        if (event.getRepeatCount() != 0) {
            return KeyEventPreHandleResult.NOT_HANDLED;
        }
        return BrowserShortcutManagerJni.get()
                .preHandleKeyEvent(mNativeBrowserShortcutManagerAndroid, event);
    }

    /**
     * Handles {@code event} after focused Android Views and web content decline it, executing any
     * matching non-reserved browser shortcut or window accelerator.
     *
     * @param event The incoming key event.
     * @return {@code true} if the event was handled.
     */
    public boolean handleKeyEvent(KeyEvent event) {
        assert mNativeBrowserShortcutManagerAndroid != 0;
        return mAcceleratorManager.processKeyEvent(event);
    }

    /**
     * Unregisters accelerators and destroys the native {@code BrowserShortcutManagerAndroid}. Must
     * be called before the window's {@link AcceleratorManager} is destroyed.
     */
    public void destroy() {
        if (mNativeBrowserShortcutManagerAndroid != 0) {
            BrowserShortcutManagerJni.get().destroy(mNativeBrowserShortcutManagerAndroid);
            mNativeBrowserShortcutManagerAndroid = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
                BrowserCommandDelegate commandDelegate);

        void destroy(long nativeBrowserShortcutManagerAndroid);

        @KeyEventPreHandleResult
        int preHandleKeyEvent(long nativeBrowserShortcutManagerAndroid, KeyEvent event);
    }
}
