// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.Surface;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/** Collection of methods that differ in the implementation per OS build version. */
abstract class PartialCustomTabVersionCompat {
    protected final Activity mActivity;
    protected final Runnable mPositionUpdater;

    static PartialCustomTabVersionCompat create(Activity activity, Runnable positionUpdater) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return new PartialCustomTabVersionCompatLegacy(activity, positionUpdater);
        }
        return new PartialCustomTabVersionCompatR(activity, positionUpdater);
    }

    /** Updates the tab's height/size after user interaction/device rotation */
    abstract void updatePosition();

    /** Returns display height */
    abstract @Px int getDisplayHeight();

    /** Returns display width */
    abstract @Px int getDisplayWidth();

    /** Returns display width in dp */
    abstract int getDisplayWidthDp();

    /** Returns screen width including system UI area. */
    abstract @Px int getScreenWidth();

    /** Returns the status bar height */
    abstract @Px int getStatusbarHeight();

    /** Returns the bottom navigation bar height */
    abstract @Px int getNavbarHeight();

    /** Offset of x with respect to the origin, where the content area begins. */
    abstract @Px int getXOffset();

    /**
     * Sets the callback to invoke when IME (soft keyboard) visible state is updated.
     * @param callback Callback to invoke upon IME state update. Can be {@code null} to
     *        remove the callback.
     * @return {@code true} if setting (or removing) happened as expected.
     */
    abstract boolean setImeStateCallback(@Nullable Callback<Boolean> callback);

    /** Implementation that supports R+ */
    @RequiresApi(Build.VERSION_CODES.R)
    private static class PartialCustomTabVersionCompatR extends PartialCustomTabVersionCompat {
        private WindowInsetsAnimation.Callback mAnimCallback;

        private PartialCustomTabVersionCompatR(Activity activity, Runnable positionUpdater) {
            super(activity, positionUpdater);
        }

        @Override
        void updatePosition() {
            mPositionUpdater.run();
        }

        @Override
        @Px
        int getDisplayHeight() {
            return windowBounds().height();
        }

        @Override
        @Px
        int getDisplayWidth() {
            Insets navbarInsets =
                    mActivity
                            .getWindowManager()
                            .getCurrentWindowMetrics()
                            .getWindowInsets()
                            .getInsets(
                                    WindowInsets.Type.navigationBars()
                                            | WindowInsets.Type.displayCutout());
            int navbarWidth = navbarInsets.left + navbarInsets.right;
            return windowBounds().width() - navbarWidth;
        }

        @Override
        @Px
        int getScreenWidth() {
            return windowBounds().width();
        }

        private Rect windowBounds() {
            return mActivity.getWindowManager().getCurrentWindowMetrics().getBounds();
        }

        @Override
        int getDisplayWidthDp() {
            return (int) (getDisplayWidth() / mActivity.getResources().getDisplayMetrics().density);
        }

        @Override
        @Px
        int getStatusbarHeight() {
            return mActivity
                    .getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.statusBars())
                    .top;
        }

        @Override
        @Px
        int getNavbarHeight() {
            return navigationBarInsets().bottom;
        }

        @Override
        @Px
        int getXOffset() {
            return navigationBarInsets().left;
        }

        private Insets navigationBarInsets() {
            return mActivity
                    .getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.navigationBars());
        }

        @Override
        boolean setImeStateCallback(Callback<Boolean> callback) {
            boolean update = (callback == null) ^ (mAnimCallback == null);
            if (callback == null && mAnimCallback != null) {
                mAnimCallback = null;
            } else if (callback != null && mAnimCallback == null) {
                mAnimCallback =
                        new WindowInsetsAnimation.Callback(
                                WindowInsetsAnimation.Callback.DISPATCH_MODE_STOP) {
                            @Override
                            public WindowInsets onProgress(
                                    @NonNull WindowInsets insets,
                                    @NonNull List<WindowInsetsAnimation> runningAnimations) {
                                return insets;
                            }

                            @Override
                            public void onEnd(@NonNull WindowInsetsAnimation animation) {
                                WindowInsets insets =
                                        mActivity
                                                .getWindowManager()
                                                .getCurrentWindowMetrics()
                                                .getWindowInsets();
                                callback.onResult(insets.isVisible(WindowInsets.Type.ime()));
                            }
                        };
            }
            if (update) {
                View view = mActivity.getWindow().getDecorView();
                view.setWindowInsetsAnimationCallback(mAnimCallback);
            }
            return update;
        }
    }

    PartialCustomTabVersionCompat(Activity activity, Runnable positionUpdater) {
        mActivity = activity;
        mPositionUpdater = positionUpdater;
    }

    /** Implementation that supports version below R */
    private static class PartialCustomTabVersionCompatLegacy extends PartialCustomTabVersionCompat {
        private View.OnLayoutChangeListener mLayoutListener;

        private PartialCustomTabVersionCompatLegacy(Activity activity, Runnable positionUpdater) {
            super(activity, positionUpdater);
        }

        @Override
        void updatePosition() {
            // On pre-R devices, We wait till the layout is complete and get the content
            // |android.R.id.content| view height. See |getAppUsableScreenHeightFromContent|.
            View contentFrame = mActivity.findViewById(android.R.id.content);

            // Maybe invoked before layout inflation? Simply return here - position update will be
            // attempted later again by |onPostInflationStartUp|.
            if (contentFrame == null) return;

            contentFrame.addOnLayoutChangeListener(
                    new View.OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(
                                View v,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) {
                            contentFrame.removeOnLayoutChangeListener(this);
                            mPositionUpdater.run();
                        }
                    });
        }

        // TODO(jinsukkim): Explore the way to use androidx.window.WindowManager or
        // androidx.window.java.WindowInfoRepoJavaAdapter once the androidx API get finalized and is
        // available in Chromium to use #getCurrentWindowMetrics()/#currentWindowMetrics() to get
        // the height of the display our Window currently in.
        @Override
        @Px
        int getDisplayHeight() {
            DisplayMetrics displayMetrics = getDisplayMetrics();
            return displayMetrics.heightPixels;
        }

        @Override
        @Px
        int getDisplayWidth() {
            Display display = mActivity.getWindowManager().getDefaultDisplay();
            Point size = new Point();
            display.getSize(size);
            return size.x;
        }

        @Override
        @Px
        int getScreenWidth() {
            return getDisplayMetrics().widthPixels;
        }

        @Override
        int getDisplayWidthDp() {
            DisplayMetrics displayMetrics = getDisplayMetrics();
            return (int) (getDisplayWidth() / displayMetrics.density);
        }

        @Override
        @SuppressWarnings("DiscouragedApi")
        @Px
        int getStatusbarHeight() {
            int statusBarHeight = 0;
            final int statusBarHeightResourceId =
                    mActivity.getResources().getIdentifier("status_bar_height", "dimen", "android");
            if (statusBarHeightResourceId > 0) {
                statusBarHeight =
                        mActivity.getResources().getDimensionPixelSize(statusBarHeightResourceId);
            }
            return statusBarHeight;
        }

        @Override
        @Px
        int getNavbarHeight() {
            // Pre-R OS offers no official way to get the navigation bar height. A common way was
            // to get it from a resource definition('navigation_bar_height') but it fails on some
            // vendor-customized devices.
            // A workaround here is to subtract the app-usable height from the whole display height.
            // There are a couple of ways to get the app-usable height:
            // 1) content frame + status bar height
            // 2) |display.getSize|
            // On some devices, only one returns the right height, the other returning a height
            // bigger that the actual value. Heuristically we choose the smaller of the two.
            return getDisplayHeight()
                    - Math.max(
                            getAppUsableScreenHeightFromContent(),
                            getAppUsableScreenHeightFromDisplay());
        }

        private int getAppUsableScreenHeightFromContent() {
            // A correct way to get the client area height would be to use the parent of |content|
            // to make sure to include the top action bar dimension. But CCT (or Chrome for that
            // matter) doesn't have the top action bar. So getting the height of |content| is
            // enough.
            View contentFrame = mActivity.findViewById(android.R.id.content);
            return contentFrame.getHeight() + getStatusbarHeight();
        }

        private int getAppUsableScreenHeightFromDisplay() {
            Display display = mActivity.getWindowManager().getDefaultDisplay();
            Point size = new Point();
            display.getSize(size);
            return size.y;
        }

        @Override
        @Px
        int getXOffset() {
            Display display = mActivity.getWindowManager().getDefaultDisplay();
            if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                    && display.getRotation() == Surface.ROTATION_270) {
                // On the phone in reverse-landscape mode, navigation bar is located on the left
                // side of the screen. The origin of x should be offset as much.
                // |getDisplayWidth()| already takes into account the display cutout insets on
                // both sides. Subtract the right inset since it doesn't affect the offset.
                return getScreenWidth() - getDisplayWidth() - getDisplayCutoutRightInset(display);
            }
            return 0;
        }

        private static int getDisplayCutoutRightInset(Display display) {
            // TODO(crbug.com/40898784): Make this work on P.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return 0;
            DisplayCutout cutout = display.getCutout();
            return cutout != null ? cutout.getSafeInsetRight() : 0;
        }

        @Override
        boolean setImeStateCallback(Callback<Boolean> callback) {
            View contentFrame = mActivity.findViewById(android.R.id.content);
            if (callback == null && mLayoutListener != null) {
                contentFrame.removeOnLayoutChangeListener(mLayoutListener);
                mLayoutListener = null;
                return true;
            } else if (callback != null && mLayoutListener == null) {
                // Ignores the callback if already added.
                mLayoutListener =
                        (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                            if (oldBottom - oldTop >= bottom - top) return;

                            // Note that keyboard visibility might not always be correct - i.e. can
                            // be false when it is visible. In worst case, tab is back to initial
                            // height and remains hidden by the keyboard. Users either have to
                            // dismiss the keyboard, or expand the tab (in non-fixed-height mode)
                            // to use it again.
                            boolean imeVisible =
                                    KeyboardVisibilityDelegate.getInstance()
                                            .isKeyboardShowing(mActivity, view);
                            callback.onResult(imeVisible);
                        };
                contentFrame.addOnLayoutChangeListener(mLayoutListener);
                return true;
            }
            return false; // adding or removing did not happen
        }

        // Determines how to gather display metrics depending if in multi-window mode or not
        //
        // The #getRealMetrics() method will give the physical size of the screen, which is
        // generally fine when the app is not in multi-window mode and #getMetrics() will give the
        // height excluding the decor views, so not suitable for our case. But in multi-window mode,
        // we do not have much choice, the closest way is to use #getMetrics() method, because we
        // need to handle rotation.
        private DisplayMetrics getDisplayMetrics() {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            if (MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
                mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            } else {
                mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
            }

            return displayMetrics;
        }
    }
}
