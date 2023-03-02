// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Handler;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.Toast.ToastPriority;

import java.util.function.BooleanSupplier;

/**
 * Interface for fullscreen notification toast that allows experimenting different
 * implementations, based on Android Toast widget and a custom view.
 */
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

        private Toast mNotificationToast;

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

            int resId = R.string.immersive_fullscreen_api_notification;
            mNotificationToast = Toast.makeTextWithPriority(
                    mActivity, resId, Toast.LENGTH_LONG, ToastPriority.HIGH);
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

    // Custom view-based fullscreen toast.
    class CustomViewToast implements FullscreenToast {
        // Fade in/out animation duration for fullscreen notification toast.
        private static final int TOAST_FADE_MS = 500;

        // Time that the notification toast remains on-screen before starting to fade out.
        private static final int TOAST_SHOW_DURATION_MS = 5000;

        private final Activity mActivity;
        private final Handler mHandler;
        private final Supplier<Tab> mTab;
        private final Supplier<Tab> mTabInFullscreen;
        private final Supplier<View> mContentViewInFullscreen;

        // Runnable that will complete the current toast and fade it out.
        private final Runnable mFadeOutNotificationToastRunnable;

        // Toast at the top of the screen that is shown when user enters fullscreen for the
        // first time.
        //
        // This is whether we believe that we need to show the user a notification toast.  It's
        // false if we're not in full screen, or if we are in full screen but have already shown the
        // toast for enough time for the user to read it.  The toast might or might not actually be
        // on-screen right now; we remove it in some cases like when we lose window focus.  However,
        // as long as we'll be in full screen, we still keep the toast pending until we successfully
        // show it.
        private boolean mIsNotificationToastPending;

        // Sometimes, the toast must be removed temporarily, such as when we lose focus or if we
        // transition to picture-in-picture.  In those cases, the toast is removed from the view
        // hierarchy, and these fields are cleared.  The toast will be re-created from scratch when
        // it's appropriate to show it again.  `mIsNotificationToastPending` won't be reset in those
        // cases, though, since we'll still want to show the toast when it's possible to do so.
        //
        // If `mNotificationToast` exists, then it's attached to the view hierarchy, though it might
        // be animating to or from alpha=0.  Any time the toast exists, we also have an animation
        // for it, to allow us to fade it in, and eventually back out.  The animation is not cleared
        // when it completes; it's only cleared when we also detach the toast and clear
        // `mNotificationToast`.
        //
        // Importantly, it's possible that `mNotificationToast` is not null while no toast is
        // pending. This can happen when the toast has been on-screen long enough, and is fading
        // out.
        private View mNotificationToast;
        private ViewPropertyAnimator mToastFadeAnimation;

        private DimensionCompat mDimensionCompat;
        private int mNavbarHeight;

        // Monitors the window layout change while the fullscreen toast is on.
        private OnGlobalLayoutListener mWindowLayoutListener = new OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                if (mContentViewInFullscreen.get() == null || mNotificationToast == null) return;
                Rect bounds = new Rect();
                mContentViewInFullscreen.get().getWindowVisibleDisplayFrame(bounds);
                var lp = (ViewGroup.MarginLayoutParams) mNotificationToast.getLayoutParams();
                int bottomMargin = mContentViewInFullscreen.get().getHeight() - bounds.height();
                // If positioned at the bottom of the display, shift it up to avoid overlapping
                // with the bottom nav bar when it appears by user gestures.
                if (bottomMargin == 0) bottomMargin = mNavbarHeight;
                lp.setMargins(0, 0, 0, bottomMargin);
                mNotificationToast.requestLayout();
            }
        };

        CustomViewToast(Activity activity, Handler handler, Supplier<Tab> tab,
                Supplier<View> contentViewInFullscreen, Supplier<Tab> tabInFullscreen) {
            mActivity = activity;
            mHandler = handler;
            mTab = tab;
            mContentViewInFullscreen = contentViewInFullscreen;
            mTabInFullscreen = tabInFullscreen;
            mFadeOutNotificationToastRunnable = this::fadeOutNotificationToast;
            mDimensionCompat = DimensionCompat.create(mActivity, () -> {});
        }

        @Override
        public void onEnterFullscreen() {
            // Cache the navigation bar height before entering fullscreen mode in which
            // the dimension is zero.
            mNavbarHeight = mDimensionCompat.getNavbarHeight();
            mActivity.getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(
                    mWindowLayoutListener);
        }

        @Override
        public void onFullscreenLayout() {
            beginNotificationToast();
        }

        @Override
        public void onExitPersistentFullscreen() {
            cancelNotificationToast();
        }

        @Override
        public void onExitFullscreen() {
            cancelNotificationToast();
        }

        /**
         * Whether we show a toast message when entering fullscreen.
         */
        private boolean shouldShowToast() {
            // If there's no notification toast pending, such as when we're not in full screen or
            // after we've already displayed it for longe enough, then we don't need to show the
            // toast now.
            if (!mIsNotificationToastPending) return false;

            if (mTabInFullscreen.get() == null) return false;

            if (mTab.get() == null) return false;

            final ViewGroup parent = mTab.get().getContentView();
            if (parent == null) return false;

            // The window must have the focus, so that it is not obscured while the notification is
            // showing.  This also covers the case of picture in picture video, but any case of an
            // unfocused window should prevent the toast.
            if (!parent.hasWindowFocus()) return false;

            return true;
        }

        /**
         * Create and show the fullscreen notification toast, if it's not already visible and if it
         * should be visible.  It's okay to call this when it should not be; we'll do nothing.  This
         * will fade the toast in if needed.  It will also schedule a timer to fade it back out, if
         * it's not hidden or cancelled before then.
         */
        private void createAndShowNotificationToast() {
            // If it's already visible, then that's fine.  That includes if it's currently fading
            // out; that's part of it.
            if (mNotificationToast != null) return;

            // If the toast should not be visible, then do nothing.
            if (!shouldShowToast()) return;

            assert mTab.get() != null && mTab.get().getContentView() != null;

            // Create a new toast and fade it in, or re-use one we've created before.
            mNotificationToast = mActivity.getWindow().findViewById(R.id.fullscreen_notification);
            boolean addView = false;
            if (mNotificationToast == null) {
                mNotificationToast = LayoutInflater.from(mActivity).inflate(
                        R.layout.fullscreen_notification, null);
                addView = true;
            }
            mNotificationToast.setAlpha(0);
            mToastFadeAnimation = mNotificationToast.animate();
            if (addView) {
                mActivity.addContentView(mNotificationToast,
                        new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
                // Ensure the toast is visible on bottom sheet CCT which is elevated for shadow
                // effect. Does no harm on other embedders.
                mNotificationToast.setElevation(mActivity.getResources().getDimensionPixelSize(
                        R.dimen.fullscreen_toast_elevation));
            } else {
                mNotificationToast.setVisibility(View.VISIBLE);
            }

            mToastFadeAnimation.alpha(1).setDuration(TOAST_FADE_MS).start();
            mHandler.postDelayed(mFadeOutNotificationToastRunnable, TOAST_SHOW_DURATION_MS);
        }

        /**
         * Pause the notification toast, which hides it and stops all the timers.  It's okay if
         * there is not currently a toast; we don't change any state in that case.  This will
         * abruptly hide the toast, rather than fade it out.  This does not change
         * `mIsNotificationToastPending`; the toast hasn't been shown long enough.
         */
        private void hideImmediatelyNotificationToast() {
            if (mNotificationToast == null) return;

            // Stop the fade-out timer.
            mHandler.removeCallbacks(mFadeOutNotificationToastRunnable);

            // Remove it immediately, without fading out.
            assert mToastFadeAnimation != null;
            mToastFadeAnimation.cancel();
            mToastFadeAnimation = null;

            mActivity.getWindow().getDecorView().getViewTreeObserver().removeOnGlobalLayoutListener(
                    mWindowLayoutListener);

            // We can't actually remove it, so this will do.
            mNotificationToast.setVisibility(View.GONE);
            mNotificationToast = null;
        }

        /**
         * Begin a new instance of the notification toast.  If the toast should not be shown right
         * now, we'll start showing it when we can.
         */
        private void beginNotificationToast() {
            // It would be nice if we could determine that we're not starting a new toast while a
            // previous one is fading out.  We can't ask the animation for its current target value.
            // We could almost check that there's not a notification pending and also that there's
            // no current toast.  When a notification is pending, the previous toast hasn't
            // completed yet, so nobody should be starting a new one.  When `mNotificationToast` is
            // not null, but pending is false, then the fade-out animation has started but not
            // completed.  Only when they're both false is it in the steady-state of "no
            // notification" that would let us start a new one.
            //
            // The problem with that is that there are cases when we double-enter fullscreen.  In
            // particular, changing the visibility of the navigation bar and/or status bar can cause
            // us to think that we're entering fullscreen without an intervening exit.  In this
            // case, the right thing to do is to continue with the toast from the previous full
            // screen, if it's still on-screen.  If it's fading out now, just let it continue to
            // fade out.  The user has already seen it for the full duration, and we've not actually
            // exited fullscreen.
            if (mNotificationToast != null) {
                // Don't reset the pending flag here -- either it's on the screen or fading out, and
                // either way is correct.  We have not actually exited fullscreen, so we shouldn't
                // re-display the notification.
                return;
            }

            mIsNotificationToastPending = true;
            createAndShowNotificationToast();
        }

        /**
         * Cancel a toast immediately, without fading out.  For example, if we leave fullscreen,
         * then the toast isn't needed anymore.
         */
        private void cancelNotificationToast() {
            hideImmediatelyNotificationToast();
            // Don't restart it either.
            mIsNotificationToastPending = false;
        }

        /**
         * Called when the notification toast should not be shown any more, because it's been
         * on-screen long enough for the user to read it.  To re-show it, one must call
         * `beginNotificationToast()` again.  Show / hide of the toast will no-op until then.
         */
        private void fadeOutNotificationToast() {
            if (mNotificationToast == null) return;

            // Clear this first, so that we know that the toast timer has expired already.
            mIsNotificationToastPending = false;

            // Cancel any timer that will start the fade-out animation, in case it's running.  It
            // might not be, especially if we're called by it.
            mHandler.removeCallbacks(mFadeOutNotificationToastRunnable);

            // Start the fade-out animation.
            assert mToastFadeAnimation != null;
            mToastFadeAnimation.cancel();
            mToastFadeAnimation.alpha(0)
                    .setDuration(TOAST_FADE_MS)
                    .withEndAction(this::hideImmediatelyNotificationToast)
                    .start();
        }

        @Override
        public void onWindowFocusChanged(boolean hasWindowFocus) {
            // Try to show / hide the toast, if we need to.  Note that these won't do anything if
            // the toast should not be visible, such as if we re-gain the window focus after having
            // completed the most recently started notification toast.
            //
            // Also note that this handles picture-in-picture.  We definitely do not want the toast
            // to be visible then; it's not relevant and also takes up almost all of the window.  We
            // could also do this on ActivityStateChanged => PAUSED if
            // Activity.isInPictureInPictureMode(), but it doesn't seem to be needed.
            if (hasWindowFocus) {
                createAndShowNotificationToast();
            } else {
                // While we don't have the focus, hide any ongoing notification.
                hideImmediatelyNotificationToast();
            }
        }

        @Override
        public boolean isVisible() {
            return mNotificationToast != null;
        }

        boolean isVisibleForTesting() {
            return mNotificationToast != null;
        }

        int getToastBottomMarginForTesting() {
            var lp = (ViewGroup.MarginLayoutParams) mNotificationToast.getLayoutParams();
            return lp.bottomMargin;
        }

        void setDimensionCompatForTesting(DimensionCompat compat) {
            mDimensionCompat = compat;
        }

        void triggerWindowLayoutForTesting() {
            mWindowLayoutListener.onGlobalLayout();
        }
    }
}
