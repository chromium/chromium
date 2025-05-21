// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;
import android.view.Gravity;

import org.chromium.base.BuildInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.Toast.ToastPriority;

import java.util.function.BooleanSupplier;

/**
 * Interface for fullscreen notification toast that allows experimenting different
 * implementations, based on Android Toast widget and a custom view.
 */
@NullMarked
interface FullscreenToast {
    // Fullscreen is entered. System UI starts being hidden. Actual fullscreen layout is
    // completed at |onFullscreenLayout|.
    void onEnterFullscreen();

    // Fullscreen layout is completed after system UI is hidden.
    void onFullscreenLayout();

    // Browser controls start being restored as a part of fullscreen exit process.
    void onExitPersistentFullscreen();

    // Fullscreen is exited.
    void onExitFullscreen();

    // Window focus is either gained or lost.
    void onWindowFocusChanged(boolean hasWindowFocus);

    // Whether the toast is currently visible. Test only.
    boolean isVisible();

    // Android widget-based fullscreen toast.
    static class AndroidToast implements FullscreenToast {
        private final Activity mActivity;
        private final BooleanSupplier mIsPersistentFullscreenMode;

        private @Nullable Toast mNotificationToast;

        AndroidToast(Activity activity, BooleanSupplier isPersistentFullscreenMode) {
            mActivity = activity;
            mIsPersistentFullscreenMode = isPersistentFullscreenMode;
        }

        @Override
        public void onExitPersistentFullscreen() {
            // We do not want to hide the notification toast here. Doing it in |exitFullscreen()| is
            // sufficient.
        }

        @Override
        public void onEnterFullscreen() {}

        @Override
        public void onFullscreenLayout() {
            showNotificationToast();
        }

        @Override
        public void onExitFullscreen() {
            hideNotificationToast();
        }

        @Override
        public void onWindowFocusChanged(boolean hasWindowFocus) {
            if (hasWindowFocus && mIsPersistentFullscreenMode.getAsBoolean()) {
                showNotificationToast();
            } else {
                hideNotificationToast();
            }
        }

        @Override
        public boolean isVisible() {
            return mNotificationToast != null;
        }

        private void showNotificationToast() {
            hideNotificationToast();

            int toastTextId =
                    UiUtils.isGestureNavigationMode(mActivity.getWindow())
                            ? R.string.immersive_fullscreen_gesture_navigation_mode_api_notification
                            : R.string.immersive_fullscreen_api_notification;
            if (BuildInfo.getInstance().isDesktop) {
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DISPLAY_EDGE_TO_EDGE_FULLSCREEN)) {
                    toastTextId = R.string.immersive_fullscreen_api_notification_desktop;
                }
            }
            if (BuildInfo.getInstance().isAutomotive) {
                toastTextId = R.string.immersive_fullscreen_automotive_toolbar_improvements;
            }
            mNotificationToast =
                    Toast.makeTextWithPriority(
                            mActivity, toastTextId, Toast.LENGTH_LONG, ToastPriority.HIGH);
            mNotificationToast.setGravity(Gravity.BOTTOM | Gravity.CENTER, 0, 0);
            mNotificationToast.show();
        }

        private void hideNotificationToast() {
            if (mNotificationToast != null) {
                mNotificationToast.cancel();
                mNotificationToast = null;
            }
        }
    }
}
