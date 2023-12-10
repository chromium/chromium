// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ALL_UPDATES;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowManager;
import android.view.animation.AnimationUtils;
import android.widget.PopupWindow;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ObserverList;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;
import org.chromium.ui.resources.HandleViewResources;
import org.chromium.ui.touch_selection.TouchHandleOrientation;

import java.util.Collections;

/**
 * View that displays a selection or insertion handle for text editing.
 *
 * While a HandleView is logically a child of some other view, it does not exist in that View's
 * hierarchy.
 *
 */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class PopupTouchHandleDrawable extends View implements DisplayAndroidObserver {
    private final PopupWindow mContainer;
    private final ViewPositionObserver.Listener mParentPositionListener;
    private WebContents mWebContents;
    private ViewGroup mContainerView;
    private ViewPositionObserver mParentPositionObserver;
    private Drawable mDrawable;

    // The native side of this object.
    private final long mNativeDrawable;

    // The position of the handle relative to the parent view in DIP.
    private float mOriginXDip;
    private float mOriginYDip;

    // The position of the parent view relative to the application's root view in pixels.
    private int mParentPositionX;
    private int mParentPositionY;

    // The mirror values based on which the handles are inverted about X and Y axes.
    private boolean mMirrorHorizontal;
    private boolean mMirrorVertical;

    private float mAlpha;

    private final int[] mTempScreenCoords = new int[2];

    private int mOrientation = TouchHandleOrientation.UNDEFINED;

    private float mDeviceScale;

    // Length of the delay before fading in after the last page movement.
    private static final int MOVING_FADE_IN_DELAY_MS = 300;
    private static final int FADE_IN_DURATION_MS = 200;
    private Runnable mDeferredHandleFadeInRunnable;
    private long mFadeStartTime;
    private Runnable mTemporarilyHiddenExpiredRunnable;
    private long mTemporarilyHiddenExpireTime;
    private boolean mVisible;
    private boolean mScrolling;
    private boolean mFocused;
    private boolean mTemporarilyHidden;
    private boolean mAttachedToWindow;
    private boolean mRotationChanged;

    // Gesture accounting for handle hiding while scrolling.
    private final GestureStateListener mGestureStateListener;

    // There are no guarantees that the side effects of setting the position of
    // the PopupWindow and the visibility of its content View will be realized
    // in the same frame. Thus, to ensure the PopupWindow is seen in the right
    // location, when the PopupWindow reappears we delay the visibility update
    // by one frame after setting the position.
    private boolean mDelayVisibilityUpdateWAR;

    // Deferred runnable to avoid invalidating outside of frame dispatch,
    // in turn avoiding issues with sync barrier insertion.
    private Runnable mInvalidationRunnable;

    private boolean mNeedsUpdateDrawable;

    // List of drawables used to inform them of the container view switching.
    private final ObserverList<PopupTouchHandleDrawable> mDrawableObserverList;

    private PopupTouchHandleDrawable(
            ObserverList<PopupTouchHandleDrawable> drawableObserverList,
            WebContents webContents,
            ViewGroup containerView) {
        super(containerView.getContext());
        mDrawableObserverList = drawableObserverList;
        mDrawableObserverList.addObserver(this);

        mWebContents = webContents;
        mContainerView = containerView;

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        mDeviceScale = windowAndroid.getDisplay().getDipScale();

        mContainer =
                new PopupWindow(
                        windowAndroid.getContext().get(),
                        null,
                        android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);

        // The built-in PopupWindow animation causes jank when transitioning between
        // visibility states. We use a custom fade-in animation when necessary.
        mContainer.setAnimationStyle(0);

        // The SUB_PANEL window layout type improves z-ordering with respect to
        // other popup-based elements.
        mContainer.setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        mAlpha = 0.f;
        mVisible = false;
        setVisibility(INVISIBLE);
        mFocused = containerView.hasWindowFocus();

        mParentPositionObserver = new ViewPositionObserver(containerView);
        mParentPositionListener = (x, y) -> updateParentPosition(x, y);
        mGestureStateListener =
                new GestureStateListener() {
                    @Override
                    public void onScrollStarted(
                            int scrollOffsetX, int scrollOffsetY, boolean isDirectionUp) {
                        setIsScrolling(true);
                    }

                    @Override
                    public void onScrollEnded(int scrollOffsetX, int scrollOffsetY) {
                        setIsScrolling(false);
                    }

                    @Override
                    public void onFlingStartGesture(
                            int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                        // Fling accounting is unreliable in WebView, as the embedder
                        // can override onScroll() and suppress fling ticking. At best
                        // we have to rely on the scroll offset changing to temporarily
                        // and repeatedly keep the handles hidden.
                        setIsScrolling(false);
                    }

                    @Override
                    public void onScrollOffsetOrExtentChanged(
                            int scrollOffsetY, int scrollExtentY) {
                        temporarilyHide();
                    }

                    @Override
                    public void onWindowFocusChanged(boolean hasWindowFocus) {
                        setIsFocused(hasWindowFocus);
                    }

                    @Override
                    public void onDestroyed() {
                        destroy();
                    }
                };
        GestureListenerManager.fromWebContents(mWebContents)
                .addListener(mGestureStateListener, ALL_UPDATES);
        mNativeDrawable =
                PopupTouchHandleDrawableJni.get()
                        .init(
                                PopupTouchHandleDrawable.this,
                                HandleViewResources.getHandleHorizontalPaddingRatio());
    }

    public static PopupTouchHandleDrawable create(
            ObserverList<PopupTouchHandleDrawable> drawableObserverList,
            WebContents webContents,
            ViewGroup containerView) {
        return new PopupTouchHandleDrawable(drawableObserverList, webContents, containerView);
    }

    public long getNativeDrawable() {
        return mNativeDrawable;
    }

    private static Drawable getHandleDrawable(Context context, int orientation) {
        switch (orientation) {
            case TouchHandleOrientation.LEFT:
                {
                    return HandleViewResources.getLeftHandleDrawable(context);
                }

            case TouchHandleOrientation.RIGHT:
                {
                    return HandleViewResources.getRightHandleDrawable(context);
                }

            case TouchHandleOrientation.CENTER:
                {
                    return HandleViewResources.getCenterHandleDrawable(context);
                }

            case TouchHandleOrientation.UNDEFINED:
            default:
                assert false;
                return HandleViewResources.getCenterHandleDrawable(context);
        }
    }

    // Implements DisplayAndroidObserver
    @Override
    public void onRotationChanged(int rotation) {
        mRotationChanged = true;
    }

    // Implements DisplayAndroidObserver
    @Override
    public void onDIPScaleChanged(float dipScale) {
        if (mDeviceScale != dipScale) {
            mDeviceScale = dipScale;

            // Postpone update till onConfigurationChanged()
            mNeedsUpdateDrawable = true;
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mWebContents == null) return false;
        // Convert from PopupWindow local coordinates to parent view local coordinates prior to
        // forwarding.
        mContainerView.getLocationOnScreen(mTempScreenCoords);
        final float offsetX = event.getRawX() - event.getX() - mTempScreenCoords[0];
        final float offsetY = event.getRawY() - event.getY() - mTempScreenCoords[1];
        final MotionEvent offsetEvent = MotionEvent.obtainNoHistory(event);
        offsetEvent.offsetLocation(offsetX, offsetY);
        final boolean handled = mWebContents.getEventForwarder().onTouchHandleEvent(offsetEvent);
        offsetEvent.recycle();
        return handled;
    }

    @CalledByNative
    private void setOrientation(int orientation, boolean mirrorVertical, boolean mirrorHorizontal) {
        assert (orientation >= TouchHandleOrientation.LEFT
                && orientation <= TouchHandleOrientation.UNDEFINED);

        final boolean orientationChanged = mOrientation != orientation;
        final boolean mirroringChanged =
                mMirrorHorizontal != mirrorHorizontal || mMirrorVertical != mirrorVertical;
        mOrientation = orientation;
        mMirrorHorizontal = mirrorHorizontal;
        mMirrorVertical = mirrorVertical;

        // Create new InvertedDrawable only if orientation has changed.
        // Otherwise, change the mirror values to scale canvas on draw() calls.
        if (orientationChanged) mDrawable = getHandleDrawable(getContext(), mOrientation);

        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));

        if (orientationChanged || mirroringChanged) scheduleInvalidate();
    }

    private void updateDrawableAndRequestLayout() {
        mNeedsUpdateDrawable = false;

        if (mDrawable == null) return;

        mDrawable = getHandleDrawable(getContext(), mOrientation);

        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));

        if (!isInLayout()) {
            ViewUtils.requestLayout(
                    this, "PopupTouchHandleDrawable.updateDrawableAndRequestLayout");
        }
    }

    private void updateParentPosition(int parentPositionX, int parentPositionY) {
        if (mParentPositionX == parentPositionX && mParentPositionY == parentPositionY) return;
        mParentPositionX = parentPositionX;
        mParentPositionY = parentPositionY;
        temporarilyHide();
    }

    private int getContainerPositionX() {
        return mParentPositionX + (int) (mOriginXDip * mDeviceScale);
    }

    private int getContainerPositionY() {
        return mParentPositionY + (int) (mOriginYDip * mDeviceScale);
    }

    private void updatePosition() {
        // Do not update the position when the handle is hidden, because PopupWindow.update()
        // will trigger android wm performLayout in the same doFrame(), which is a serious waste
        // of CPU and easy to cause page jitter when scrolling, the user experience become worse.
        if (getVisibility() != VISIBLE) {
            // This does not affect the TextMagnifier feature, because that once the first
            // MotionEvent is passed to this PopupWindow, if we don't lift finger, the following
            // MotionEvents are still passed to PopupWindow, so even the PopupWindow isn't moving
            // follow finger, it still can correctly pass events to EventForwarder. Therefore the
            // cursor/selection is still correctly adjusted.
            return;
        }
        mContainer.update(
                getContainerPositionX(),
                getContainerPositionY(),
                getRight() - getLeft(),
                getBottom() - getTop());
    }

    private boolean isShowingAllowed() {
        // mOriginX/YDip * deviceScale is the distance of the handler drawable from the top left of
        // the containerView (the WebView).
        return mAttachedToWindow
                && mVisible
                && mFocused
                && !mScrolling
                && !mTemporarilyHidden
                && isPositionVisible(mOriginXDip * mDeviceScale, mOriginYDip * mDeviceScale);
    }

    // Adapted from android.widget.Editor#isPositionVisible.
    private boolean isPositionVisible(final float positionX, final float positionY) {
        final float[] position = new float[2];
        position[0] = positionX;
        position[1] = positionY;
        View view = mContainerView;

        while (view != null) {
            if (view != mContainerView) {
                // Local scroll is already taken into account in positionX/Y
                position[0] -= view.getScrollX();
                position[1] -= view.getScrollY();
            }

            final float drawableWidth = mDrawable.getIntrinsicWidth();
            final float drawableHeight = mDrawable.getIntrinsicHeight();

            if (position[0] + drawableWidth < 0
                    || position[1] + drawableHeight < 0
                    || position[0] > view.getWidth()
                    || position[1] > view.getHeight()) {
                return false;
            }

            if (!view.getMatrix().isIdentity()) {
                view.getMatrix().mapPoints(position);
            }

            position[0] += view.getLeft();
            position[1] += view.getTop();

            final ViewParent parent = view.getParent();
            if (parent instanceof View) {
                view = (View) parent;
            } else {
                // We've reached the ViewRoot, stop iterating
                view = null;
            }
        }

        // We've been able to walk up the view hierarchy and the position was never clipped
        return true;
    }

    private void updateVisibility() {
        int newVisibility = isShowingAllowed() ? VISIBLE : INVISIBLE;

        // When regaining visibility, delay the visibility update by one frame
        // to ensure the PopupWindow has first been positioned properly.
        if (newVisibility == VISIBLE && getVisibility() != VISIBLE) {
            if (!mDelayVisibilityUpdateWAR) {
                mDelayVisibilityUpdateWAR = true;
                scheduleInvalidate();
                return;
            }
        }
        mDelayVisibilityUpdateWAR = false;

        setVisibility(newVisibility);
    }

    private void setIsScrolling(boolean scrolling) {
        if (mScrolling == scrolling) return;
        mScrolling = scrolling;
        onVisibilityInputChanged();
    }

    private void setIsFocused(boolean focused) {
        if (mFocused == focused) return;
        mFocused = focused;
        onVisibilityInputChanged();
    }

    private void setTemporarilyHidden(boolean hidden) {
        if (mTemporarilyHidden == hidden) return;

        mTemporarilyHidden = hidden;
        if (mTemporarilyHidden) {
            if (mTemporarilyHiddenExpiredRunnable == null) {
                mTemporarilyHiddenExpiredRunnable = () -> setTemporarilyHidden(false);
            }
            removeCallbacks(mTemporarilyHiddenExpiredRunnable);
            long now = SystemClock.uptimeMillis();
            long delay = Math.max(0, mTemporarilyHiddenExpireTime - now);
            postDelayed(mTemporarilyHiddenExpiredRunnable, delay);
        } else if (mTemporarilyHiddenExpiredRunnable != null) {
            removeCallbacks(mTemporarilyHiddenExpiredRunnable);
        }
        onVisibilityInputChanged();
    }

    private void onVisibilityInputChanged() {
        if (!mContainer.isShowing()) return;
        boolean allowed = isShowingAllowed();
        boolean wasShowingAllowed = getVisibility() == VISIBLE;
        if (wasShowingAllowed == allowed) return;
        cancelFadeIn();
        if (allowed) {
            if (mDeferredHandleFadeInRunnable == null) {
                mDeferredHandleFadeInRunnable = () -> beginFadeIn();
            }
            postOnAnimation(mDeferredHandleFadeInRunnable);
        } else {
            updateVisibility();
        }
    }

    private void updateAlpha() {
        if (mAlpha == 1.f) return;
        long currentTimeMillis = AnimationUtils.currentAnimationTimeMillis();
        mAlpha = Math.min(1.f, (float) (currentTimeMillis - mFadeStartTime) / FADE_IN_DURATION_MS);
        mDrawable.setAlpha((int) (255 * mAlpha));
        scheduleInvalidate();
    }

    private void temporarilyHide() {
        if (!mContainer.isShowing()) return;
        mTemporarilyHiddenExpireTime = SystemClock.uptimeMillis() + MOVING_FADE_IN_DELAY_MS;
        setTemporarilyHidden(true);
    }

    private void doInvalidate() {
        if (!mContainer.isShowing()) return;
        updateVisibility();
        updatePosition();
        invalidate();
    }

    private void scheduleInvalidate() {
        if (mInvalidationRunnable != null) return;

        mInvalidationRunnable =
                () -> {
                    mInvalidationRunnable = null;
                    doInvalidate();
                };
        postOnAnimation(mInvalidationRunnable);
    }

    private void cancelFadeIn() {
        if (mDeferredHandleFadeInRunnable == null) return;
        removeCallbacks(mDeferredHandleFadeInRunnable);
    }

    private void beginFadeIn() {
        if (getVisibility() == VISIBLE) return;
        mAlpha = 0.f;
        mFadeStartTime = AnimationUtils.currentAnimationTimeMillis();
        doInvalidate();
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Update the drawable when the configuration with the new density comes.
        if (mNeedsUpdateDrawable && mDeviceScale == getResources().getDisplayMetrics().density) {
            updateDrawableAndRequestLayout();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDrawable == null) {
            setMeasuredDimension(0, 0);
            return;
        }
        setMeasuredDimension(mDrawable.getIntrinsicWidth(), mDrawable.getIntrinsicHeight());
    }

    @Override
    protected void onDraw(Canvas c) {
        if (mDrawable == null) return;
        final boolean needsMirror = mMirrorHorizontal || mMirrorVertical;
        if (needsMirror) {
            c.save();
            float scaleX = mMirrorHorizontal ? -1.f : 1.f;
            float scaleY = mMirrorVertical ? -1.f : 1.f;
            c.scale(scaleX, scaleY, getWidth() / 2.0f, getHeight() / 2.0f);
        }
        updateAlpha();
        mDrawable.setBounds(0, 0, getRight() - getLeft(), getBottom() - getTop());
        mDrawable.draw(c);
        if (needsMirror) c.restore();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        // Resolve conflict with gesture navigation back when dragging this handle view on the
        // edge of the screen.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            setSystemGestureExclusionRects(Collections.singletonList(new Rect(0, 0, w, h)));
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mAttachedToWindow = true;
        onVisibilityInputChanged();

        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid != null) {
            windowAndroid.getDisplay().addObserver(this);
            mDeviceScale = windowAndroid.getDisplay().getDipScale();
            updateDrawableAndRequestLayout();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mWebContents != null) {
            WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
            if (windowAndroid != null) windowAndroid.getDisplay().removeObserver(this);
        }

        mAttachedToWindow = false;
        onVisibilityInputChanged();
    }

    @CalledByNative
    private void destroy() {
        mDrawableObserverList.removeObserver(this);
        if (mWebContents == null) return;
        hide();

        GestureListenerManager gestureManager =
                GestureListenerManager.fromWebContents(mWebContents);
        if (gestureManager != null) gestureManager.removeListener(mGestureStateListener);

        mWebContents = null;
    }

    @CalledByNative
    private void show() {
        if (mWebContents == null) return;
        if (mContainer.isShowing()) return;

        // While hidden, the parent position may have become stale. It must be updated before
        // checking isPositionVisible().
        updateParentPosition(
                mParentPositionObserver.getPositionX(), mParentPositionObserver.getPositionY());
        mParentPositionObserver.addListener(mParentPositionListener);
        mContainer.setContentView(this);
        try {
            mContainer.showAtLocation(
                    mContainerView,
                    Gravity.NO_GRAVITY,
                    getContainerPositionX(),
                    getContainerPositionY());
        } catch (WindowManager.BadTokenException e) {
            hide();
        }
    }

    @CalledByNative
    private void hide() {
        mTemporarilyHiddenExpireTime = 0;
        setTemporarilyHidden(false);
        mAlpha = 1.0f;
        if (mContainer.isShowing()) {
            try {
                mContainer.dismiss();
            } catch (IllegalArgumentException e) {
                // Intentionally swallowed due to bad Android implemention. See crbug.com/633224.
            }
        }
        mParentPositionObserver.removeListener(mParentPositionListener);
    }

    @CalledByNative
    private void setOrigin(float originXDip, float originYDip) {
        // If rotation has changed, then we always need to scheduleInvalidate() regardless of the
        // current visibility.
        if (mOriginXDip == originXDip && mOriginYDip == originYDip && !mRotationChanged) return;
        mOriginXDip = originXDip;
        mOriginYDip = originYDip;
        if (mVisible || mRotationChanged) {
            if (mRotationChanged) mRotationChanged = false;
            scheduleInvalidate();
        }
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        if (mVisible == visible) return;
        mVisible = visible;
        onVisibilityInputChanged();
    }

    @CalledByNative
    private float getOriginXDip() {
        return mOriginXDip;
    }

    @CalledByNative
    private float getOriginYDip() {
        return mOriginYDip;
    }

    @CalledByNative
    private float getVisibleWidthDip() {
        if (mDrawable == null) return 0;
        return mDrawable.getIntrinsicWidth() / mDeviceScale;
    }

    @CalledByNative
    private float getVisibleHeightDip() {
        if (mDrawable == null) return 0;
        return mDrawable.getIntrinsicHeight() / mDeviceScale;
    }

    public void onContainerViewChanged(ViewGroup newContainerView) {
        // If the parent View ever changes, the parent position observer
        // must be updated accordingly.
        mParentPositionObserver.removeListener(mParentPositionListener);
        mParentPositionObserver = new ViewPositionObserver(newContainerView);
        if (mContainer.isShowing()) {
            mParentPositionObserver.addListener(mParentPositionListener);
        }
        mContainerView = newContainerView;
    }

    @NativeMethods
    interface Natives {
        long init(PopupTouchHandleDrawable caller, float horizontalPaddingRatio);
    }
}
