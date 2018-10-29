// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.WindowInsets;

import org.chromium.base.ObserverList;

import java.lang.reflect.Method;

/**
 * The purpose of this view is to store the system window insets (OSK, status bar) for
 * later use.
 */
public class InsetObserverView extends View {
    private final Rect mWindowInsets;
    protected final ObserverList<WindowInsetObserver> mObservers;

    /**
     * Allows observing changes to the window insets from Android system UI.
     */
    public interface WindowInsetObserver {
        /**
         * Triggered when the window insets have changed.
         *
         * @param left The left inset.
         * @param top The top inset.
         * @param right The right inset (but it feels so wrong).
         * @param bottom The bottom inset.
         */
        void onInsetChanged(int left, int top, int right, int bottom);

        /** Called when a new Display Cutout safe area is applied. */
        void onSafeAreaChanged(Rect area);
    }

    /**
     * Constructs a new {@link InsetObserverView} for the appropriate Android version.
     * @param context The Context the view is running in, through which it can access the current
     *            theme, resources, etc.
     * @return an instance of a InsetObserverView.
     */
    public static InsetObserverView create(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return new InsetObserverViewApi28(context);
        } else if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return new InsetObserverView(context);
        }
        return new InsetObserverViewApi21(context);
    }

    /**
     * Creates an instance of {@link InsetObserverView}.
     * @param context The Context to create this {@link InsetObserverView} in.
     */
    public InsetObserverView(Context context) {
        super(context);
        setVisibility(INVISIBLE);
        mWindowInsets = new Rect();
        mObservers = new ObserverList<>();
    }

    /**
     * Returns the left {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsLeft() {
        return mWindowInsets.left;
    }

    /**
     * Returns the top {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsTop() {
        return mWindowInsets.top;
    }

    /**
     * Returns the right {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsRight() {
        return mWindowInsets.right;
    }

    /**
     * Returns the bottom {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsBottom() {
        return mWindowInsets.bottom;
    }

    /**
     * Add an observer to be notified when the window insets have changed.
     */
    public void addObserver(WindowInsetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of window inset changes.
     */
    public void removeObserver(WindowInsetObserver observer) {
        mObservers.removeObserver(observer);
    }

    @SuppressWarnings("deprecation")
    @Override
    protected boolean fitSystemWindows(Rect insets) {
        // For Lollipop and above, onApplyWindowInsets will set the insets.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            onInsetChanged(insets.left, insets.top, insets.right, insets.bottom);
        }
        return false;
    }

    /**
     * Updates the window insets and notifies all observers if the values did indeed change.
     *
     * @param left The updated left inset.
     * @param top The updated right inset.
     * @param right The updated right inset.
     * @param bottom The updated bottom inset.
     */
    protected void onInsetChanged(int left, int top, int right, int bottom) {
        if (mWindowInsets.left == left && mWindowInsets.top == top && mWindowInsets.right == right
                && mWindowInsets.bottom == bottom) {
            return;
        }

        mWindowInsets.set(left, top, right, bottom);

        for (WindowInsetObserver observer : mObservers) {
            observer.onInsetChanged(left, top, right, bottom);
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    protected static class InsetObserverViewApi21 extends InsetObserverView {
        /**
         * Creates an instance of {@link InsetObserverView} for Android versions L and above.
         * @param context The Context to create this {@link InsetObserverView} in.
         */
        InsetObserverViewApi21(Context context) {
            super(context);
        }

        @Override
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            onInsetChanged(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        }
    }

    // TODO(beccahughes): Remove reflection and update target API when P-SDK is landed.
    @TargetApi(Build.VERSION_CODES.O)
    @VisibleForTesting
    protected static class InsetObserverViewApi28 extends InsetObserverViewApi21 {
        private Rect mCurrentSafeArea = new Rect();

        /**
         * Creates an instance of {@link InsetObserverView} for Android versions P and above.
         * @param context The Context to create this {@link InsetObserverView} in.
         */
        InsetObserverViewApi28(Context context) {
            super(context);
        }

        @Override
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            setCurrentSafeAreaFromInsets(insets);
            return super.onApplyWindowInsets(insets);
        }

        /**
         * Get the safe area from the WindowInsets, store it and notify any observers.
         * @param insets The WindowInsets containing the safe area.
         */
        private void setCurrentSafeAreaFromInsets(WindowInsets insets) {
            Object displayCutout = extractDisplayCutout(insets);

            // Extract the safe area values.
            int left = extractSafeInset(displayCutout, "Left");
            int top = extractSafeInset(displayCutout, "Top");
            int right = extractSafeInset(displayCutout, "Right");
            int bottom = extractSafeInset(displayCutout, "Bottom");

            // If the safe area has not changed then we should stop now.
            if (mCurrentSafeArea.left == left && mCurrentSafeArea.top == top
                    && mCurrentSafeArea.right == right && mCurrentSafeArea.bottom == bottom)
                return;

            mCurrentSafeArea.set(left, top, right, bottom);

            for (WindowInsetObserver mObserver : mObservers) {
                mObserver.onSafeAreaChanged(mCurrentSafeArea);
            }
        }

        /**
         * Extracts a safe inset value from an Android P+ DisplayCutout.
         * @param displayCutout The Android P+ DisplayCutout object.
         * @param name The name of the inset to extract.
         * @return The inset value as an integer or zero.
         */
        private int extractSafeInset(Object displayCutout, String name) {
            try {
                Method method = displayCutout.getClass().getMethod("getSafeInset" + name);
                return (int) method.invoke(displayCutout);
            } catch (Exception ex) {
                // API is not available.
                return 0;
            }
        }

        /**
         * Extracts an Android P+ DisplayCutout from {@link WindowInsets}.
         * @param insets The WindowInsets to extract the cutout from.
         * @return The DisplayCutout object or null.
         */
        @VisibleForTesting
        protected Object extractDisplayCutout(WindowInsets insets) {
            try {
                Method method = insets.getClass().getMethod("getDisplayCutout");
                return method.invoke(insets);
            } catch (Exception ex) {
                // API is not available.
                return null;
            }
        }
    }
}
