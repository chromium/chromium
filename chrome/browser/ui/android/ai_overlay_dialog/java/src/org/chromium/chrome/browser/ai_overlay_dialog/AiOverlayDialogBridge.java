// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai_overlay_dialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI bridge for the AI overlay dialog.
 *
 * <p>An instance owns the window-scoped native {@code AiOverlayDialogControllerAndroid}. It is
 * registered as an Activity-scoped {@link ChromeAndroidTaskFeature}, which guarantees the native
 * controller is destroyed before the native {@code BrowserWindowInterface} it attaches itself to.
 */
@JNINamespace("ttc")
@NullMarked
public class AiOverlayDialogBridge implements ChromeAndroidTaskFeature {
    public interface AudioEnergyListener {
        void onAudioEnergyUpdated(float energy);
    }

    private long mNativeAiOverlayDialogBridge;

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        assert mNativeAiOverlayDialogBridge == 0 : "AiOverlayDialogBridge is already in a task.";
        mNativeAiOverlayDialogBridge =
                AiOverlayDialogBridgeJni.get()
                        .create(/* caller= */ this, initInfo.nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        if (mNativeAiOverlayDialogBridge != 0) {
            AiOverlayDialogBridgeJni.get().destroy(mNativeAiOverlayDialogBridge);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAiOverlayDialogBridge = 0;
    }

    /**
     * Scopes the listener to a single window: each Chrome window has its own overlay and its own
     * toolbar microphone button, so a process-wide listener would cross-wire them in multi-window.
     *
     * <p>The host holds only a {@link java.lang.ref.WeakReference} to the listener, so the listener
     * must be kept alive by its owner (the toolbar view hierarchy).
     */
    private static final UnownedUserDataKey<AudioEnergyListener> AUDIO_ENERGY_LISTENER_KEY =
            new UnownedUserDataKey<>();

    /**
     * Registers (or clears, when {@code listener} is null) the audio energy listener for {@code
     * window}.
     */
    public static void setAudioEnergyListener(
            WindowAndroid window, @Nullable AudioEnergyListener listener) {
        if (listener == null) {
            AUDIO_ENERGY_LISTENER_KEY.detachFromHost(window.getUnownedUserDataHost());
        } else {
            AUDIO_ENERGY_LISTENER_KEY.attachToHost(window.getUnownedUserDataHost(), listener);
        }
    }

    /**
     * Toggles the AI overlay for the window that owns {@code webContents}.
     *
     * <p>The overlay is per-browser-window: native resolves {@code webContents} to its tab, then to
     * that tab's window, and toggles only that window's overlay. Other windows are unaffected.
     * Callers should pass the currently active tab's WebContents.
     *
     * <p>This is best-effort and silently does nothing if the overlay cannot be resolved:
     *
     * <ul>
     *   <li>{@code webContents} is null — e.g. the toolbar has no active tab.
     *   <li>{@code webContents} is not a tab's WebContents, so it has no owning window.
     *   <li>The tab is not currently attached to a browser window.
     *   <li>The window has no overlay controller — it is not a normal browser window, or the
     *       AiOverlayDialog feature is disabled.
     *   <li>The build is not Desktop Android. This class is compiled into every Android APK because
     *       toolbar code references it, but the overlay only exists on Desktop Android; elsewhere
     *       the native side is a no-op.
     * </ul>
     */
    public static void toggleOverlay(@Nullable WebContents webContents) {
        if (webContents != null) {
            AiOverlayDialogBridgeJni.get().toggleOverlay(webContents);
        }
    }

    /**
     * Called from native when the audio energy level of {@code window}'s overlay changes.
     *
     * <p>Invoked on the browser UI thread: the caller is the AiOverlayDialog mojo page handler,
     * which is bound on that thread.
     */
    @CalledByNative
    private static void updateAudioEnergy(WindowAndroid window, float energy) {
        ThreadUtils.assertOnUiThread();
        AudioEnergyListener listener =
                AUDIO_ENERGY_LISTENER_KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        if (listener != null) {
            listener.onAudioEnergyUpdated(energy);
        }
    }

    @NativeMethods
    public interface Natives {
        /**
         * Creates the native bridge, which owns the {@code AiOverlayDialogControllerAndroid} for
         * the given window.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The pointer to the native {@code BrowserWindowInterface}.
         * @return The address of the native bridge, or 0 if the window gets no overlay (it is not a
         *     normal browser window, or this is not Desktop Android).
         */
        long create(AiOverlayDialogBridge caller, long nativeBrowserWindowPtr);

        /** Destroys the native bridge and the controller it owns. */
        void destroy(long nativeAiOverlayDialogBridge);

        void toggleOverlay(WebContents webContents);
    }
}
