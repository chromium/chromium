// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.app.Activity;

import org.jni_zero.CalledByNative;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Supplier;

/**
 * Binds a factory of {@link QuickDeleteController} instances to a {@link WindowAndroid}, so that
 * features which only have access to the window (rather than the hosting activity) can show the
 * Quick Delete dialog.
 *
 * <p>The attached {@link Supplier} is expected to create a new, fully wired {@link
 * QuickDeleteController} on each call, mirroring how the activity creates one for its app menu.
 */
@NullMarked
public final class QuickDeleteControllerProvider {
    /** The key for accessing this object on an {@link org.chromium.base.UnownedUserDataHost}. */
    private static final UnownedUserDataKey<Supplier<QuickDeleteController>> KEY =
            new UnownedUserDataKey<>();

    /**
     * Returns the {@link QuickDeleteController} factory attached to the given window, or null if
     * the window is null or none has been attached.
     *
     * @param window The window to look up the factory from.
     */
    public static @Nullable Supplier<QuickDeleteController> from(@Nullable WindowAndroid window) {
        if (window == null) return null;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Makes a {@link QuickDeleteController} factory available through the given window.
     *
     * @param window The {@link WindowAndroid} to attach to.
     * @param factory The factory to attach. The same instance must be passed to {@link #detach}.
     */
    public static void attach(WindowAndroid window, Supplier<QuickDeleteController> factory) {
        KEY.attachToHost(window.getUnownedUserDataHost(), factory);
    }

    /**
     * Detaches the given factory from any window it is attached to.
     *
     * @param factory The factory previously passed to {@link #attach}.
     */
    public static void detach(Supplier<QuickDeleteController> factory) {
        KEY.detachFromAllHosts(factory);
    }

    /**
     * Shows the Quick Delete dialog using the factory attached to the given {@link WindowAndroid},
     * if present.
     *
     * @param window The {@link WindowAndroid} to look up the factory from.
     */
    @CalledByNative
    public static void showDialog(@Nullable WindowAndroid window) {
        if (window == null) {
            return;
        }
        @Nullable Activity activity = window.getActivity().get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return;
        }
        @Nullable Supplier<QuickDeleteController> factory = from(window);
        if (factory != null) {
            factory.get().showDialog();
        }
    }

    private QuickDeleteControllerProvider() {}
}
