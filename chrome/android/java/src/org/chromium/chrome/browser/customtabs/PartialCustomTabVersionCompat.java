// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.graphics.Point;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.Px;
import androidx.annotation.RequiresApi;

import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

/**
 * Collection of methods that differ in the implementation per OS build version.
 */
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

    /** Returns the status bar height */
    abstract @Px int getStatusbarHeight();

    /** Returns the bottom navigation bar height */
    abstract @Px int getNavbarHeight();

    /** Implementation that supports R+ */
    @RequiresApi(Build.VERSION_CODES.R)
    private static class PartialCustomTabVersionCompatR extends PartialCustomTabVersionCompat {
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
            return mActivity.getWindowManager().getCurrentWindowMetrics().getBounds().height();
        }

        @Override
        @Px
        int getDisplayWidth() {
            return mActivity.getWindowManager().getCurrentWindowMetrics().getBounds().width();
        }

        @Override
        @Px
        int getStatusbarHeight() {
            return mActivity.getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.statusBars())
                    .top;
        }

        @Override
        @Px
        int getNavbarHeight() {
            return mActivity.getWindowManager()
                    .getCurrentWindowMetrics()
                    .getWindowInsets()
                    .getInsets(WindowInsets.Type.navigationBars())
                    .bottom;
        }
    }

    PartialCustomTabVersionCompat(Activity activity, Runnable positionUpdater) {
        mActivity = activity;
        mPositionUpdater = positionUpdater;
    }

    /** Implementation that supports version below R */
    private static class PartialCustomTabVersionCompatLegacy extends PartialCustomTabVersionCompat {
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

            contentFrame.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    contentFrame.removeOnLayoutChangeListener(this);
                    mPositionUpdater.run();
                }
            });
        }

        // TODO(jinsukkim): Explore the way to use androidx.window.WindowManager or
        // androidx.window.java.WindowInfoRepoJavaAdapter once the androidx API get finalized and is
        // available in Chromium to use #getCurrentWindowMetrics()/#currentWindowMetrics() to get
        // the height of the display our Window currently in.
        //
        // The #getRealMetrics() method will give the physical size of the screen, which is
        // generally fine when the app is not in multi-window mode and #getMetrics() will give the
        // height excludes the decor views, so not suitable for our case. But in multi-window mode,
        // we have no much choice, the closest way is to use #getMetrics() method, because we need
        // to handle rotation.
        @Override
        @Px
        int getDisplayHeight() {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            if (MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
                mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            } else {
                mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
            }
            return displayMetrics.heightPixels;
        }

        @Override
        @Px
        int getDisplayWidth() {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            if (MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity)) {
                mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            } else {
                mActivity.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
            }
            return displayMetrics.widthPixels;
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
                    - Math.max(getAppUsableScreenHeightFromContent(),
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
    }
}
