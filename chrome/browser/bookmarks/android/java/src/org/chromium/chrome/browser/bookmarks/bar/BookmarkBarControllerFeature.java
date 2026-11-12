// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.jni_zero.JniPtr;
import org.jni_zero.NativeMethods;

import org.chromium.bookmark_bar_api.mojom.BookmarkBarUiControllerBridge;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.ui.mojom.EventConstantsConstants;

/**
 * Owns the C++ {@code BookmarkBarUIController} of one browser window. {@link ChromeAndroidTask}
 * removes it before that window goes away.
 */
@NullMarked
class BookmarkBarControllerFeature implements ChromeAndroidTaskFeature {
    // The native `mojo::MojoJniBridge` that owns the controller, or 0.
    private long mNativeControllerBridge;
    private @Nullable BookmarkBarUiControllerBridge mControllerBridge;

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        assert mNativeControllerBridge == 0 : "BookmarkBarControllerFeature is already in a task.";
        assert initInfo.nativeBrowserWindow != null;
        mNativeControllerBridge =
                BookmarkBarControllerFeatureJni.get()
                        .createController(initInfo.nativeBrowserWindow);
        mControllerBridge = new BookmarkBarUiControllerBridge(mNativeControllerBridge);
    }

    @Override
    public void onFeatureRemoved() {
        // Never removed from within a controller call, so it's safe to destroy the controller now.
        mControllerBridge = null;
        BookmarkBarControllerFeatureJni.get().destroyController(mNativeControllerBridge);
        mNativeControllerBridge = 0;
    }

    /** Returns the controller. Don't keep it: it's invalid once the feature is removed. */
    BookmarkBarUiControllerBridge getControllerBridge() {
        return assertNonNull(mControllerBridge);
    }

    /**
     * Returns the {@code ui::EventFlags} bitmask the controller expects for a press with the given
     * Android key and mouse button states.
     */
    static int toEventFlags(int metaState, int buttonState) {
        int eventFlags = EventConstantsConstants.EVENT_FLAG_NONE;
        if ((buttonState & MotionEvent.BUTTON_TERTIARY) != 0) {
            eventFlags |= EventConstantsConstants.EVENT_FLAG_MIDDLE_MOUSE_BUTTON;
        }
        if ((metaState & KeyEvent.META_CTRL_ON) != 0) {
            eventFlags |= EventConstantsConstants.EVENT_FLAG_CONTROL_DOWN;
        }
        if ((metaState & KeyEvent.META_SHIFT_ON) != 0) {
            eventFlags |= EventConstantsConstants.EVENT_FLAG_SHIFT_DOWN;
        }
        if ((metaState & KeyEvent.META_ALT_ON) != 0) {
            eventFlags |= EventConstantsConstants.EVENT_FLAG_ALT_DOWN;
        }
        if ((metaState & KeyEvent.META_META_ON) != 0) {
            eventFlags |= EventConstantsConstants.EVENT_FLAG_COMMAND_DOWN;
        }
        return eventFlags;
    }

    @NativeMethods
    interface Natives {
        /** Returns a pointer to the native bridge, which owns the new controller. */
        long createController(JniPtr<NativeBrowserWindowInterface> browserWindow);

        void destroyController(long controllerBridge);
    }
}
