// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.features.start_surface.TabSwitcherAndStartSurfaceLayout.ZOOMING_DURATION;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.DynamicResourceReadyOnceCallback;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class.
 */
class TabListRecyclerView
        extends RecyclerView implements TabListMediator.TabGridAccessibilityHelper {
    private static final String TAG = "TabListRecyclerView";
    private static final String SHADOW_VIEW_TAG = "TabListViewShadow";

    private static final String MAX_DUTY_CYCLE_PARAM = "max-duty-cycle";
    private static final float DEFAULT_MAX_DUTY_CYCLE = 0.2f;

    public static final long BASE_ANIMATION_DURATION_MS = 218;
    public static final long FINAL_FADE_IN_DURATION_MS = 50;

    /**
     * Field trial parameter for downsampling scaling factor.
     */
    private static final String DOWNSAMPLING_SCALE_PARAM = "downsampling-scale";

    private static final float DEFAULT_DOWNSAMPLING_SCALE = 0.5f;

    /**
     * An interface to listen to visibility related changes on this {@link RecyclerView}.
     */
    interface VisibilityListener {
        /**
         * Called before the animation to show the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedShowing(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedShowing();

        /**
         * Called before the animation to hide the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedHiding(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedHiding();
    }

    private class TabListOnScrollListener extends RecyclerView.OnScrollListener {
        @Override
        public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
            final int yOffset = recyclerView.computeVerticalScrollOffset();
            if (yOffset == 0) {
                setShadowVisibility(false);
                return;
            }

            if (dy == 0 || recyclerView.getScrollState() == SCROLL_STATE_SETTLING) return;

            setShadowVisibility(yOffset > 0);
        }
    }

    // TODO(crbug.com/1076538, crbug.com/1095948): Use this ItemAnimator instead of
    // |mOriginalAnimator|, when crbug.com/1095948 has a real fix.
    @SuppressWarnings("unused")
    private class RemoveItemAnimator extends DefaultItemAnimator {
        @Override
        public boolean animateRemove(ViewHolder holder) {
            AnimatorSet scaleAnimator = new AnimatorSet();
            scaleAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    holder.itemView.setScaleX(1.0f);
                    holder.itemView.setScaleY(1.0f);
                }
            });
            ObjectAnimator scaleX = ObjectAnimator.ofFloat(holder.itemView, View.SCALE_X, 0.5f);
            ObjectAnimator scaleY = ObjectAnimator.ofFloat(holder.itemView, View.SCALE_Y, 0.5f);
            scaleX.setDuration(BASE_ANIMATION_DURATION_MS);
            scaleY.setDuration(BASE_ANIMATION_DURATION_MS);
            scaleAnimator.play(scaleX).with(scaleY);
            scaleAnimator.start();

            return super.animateRemove(holder);
        }
    }

    private final int mResourceId;
    private ValueAnimator mFadeInAnimator;
    private ValueAnimator mFadeOutAnimator;
    private VisibilityListener mListener;
    private DynamicResourceLoader mLoader;
    private ViewResourceAdapter mDynamicView;
    private boolean mIsDynamicViewRegistered;
    private long mLastDirtyTime;
    private ImageView mShadowImageView;
    private int mShadowTopOffset;
    private TabListOnScrollListener mScrollListener;
    // It is null when gts-tab animation is disabled or switching from Start surface to GTS.
    @Nullable
    private RecyclerView.ItemAnimator mOriginalAnimator;
    // Null unless item animations are disabled.
    @Nullable
    private RecyclerView.ItemAnimator mDisabledAnimatorHolder;
    // Null if there is no runnable to execute on the next layout.
    @Nullable
    private Runnable mOnNextLayoutRunnable;
    /**
     * Capture is suppressed when animations are not running. Animations are initiated after the
     * completion of {@link DynamicResource#triggerBitmapCapture()}.
     */
    private boolean mSuppressCapture = true;
    private int mToolbarHairlineColor;

    /**
     * Basic constructor to use during inflation from xml.
     */
    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);

        // Use this object in case there are multiple instances of this class.
        mResourceId = this.toString().hashCode();
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        if (mOnNextLayoutRunnable != null) {
            Runnable runnable = mOnNextLayoutRunnable;
            mOnNextLayoutRunnable = null;
            runnable.run();
        }
    }

    /**
     * Sets a runnable to start an animation that executes on next layout. This ensures any
     * positioning changes will be accounted for. If the view is not attached or will not be laid
     * out the runnable is executed immediately to avoid blocking indefinitely. This method is
     * intended to be used to defer transition animations until after a {@link DynamicView} is
     * captured.
     * @param runnable the runnable that executes on next layout.
     */
    void runAnimationOnNextLayout(Runnable runnable) {
        assert mOnNextLayoutRunnable
                == null
            : "TabListRecyclerView animation on next layout set multiple times without running.";
        mOnNextLayoutRunnable = () -> {
            if (mDynamicView == null) {
                runnable.run();
                return;
            }
            DynamicResourceReadyOnceCallback.onNext(mDynamicView, resource -> {
                mSuppressCapture = false;
                runnable.run();
            });
            mDynamicView.triggerBitmapCapture();
        };

        // If the view is detached or won't conduct a new layout then trigger the runnable
        // immediately rather than waiting for it to be attached.
        if (!isAttachedToWindow() || !isLayoutRequested()) {
            Runnable runNow = mOnNextLayoutRunnable;
            mOnNextLayoutRunnable = null;
            runNow.run();
        }
    }

    /**
     * Set the {@link VisibilityListener} that will listen on granular visibility events.
     * @param listener The {@link VisibilityListener} to use.
     */
    void setVisibilityListener(VisibilityListener listener) {
        mListener = listener;
    }

    void setDisableItemAnimations(boolean disable) {
        if (disable) {
            ItemAnimator animator = getItemAnimator();
            if (animator == null) return;

            mDisabledAnimatorHolder = animator;
            setItemAnimator(null);
        } else if (mDisabledAnimatorHolder != null) {
            setItemAnimator(mDisabledAnimatorHolder);
            mDisabledAnimatorHolder = null;
        }
    }

    void prepareTabSwitcherView() {
        endAllAnimations();

        registerDynamicView();

        // Stop all the animations to make all the items show up and scroll to position immediately.
        mOriginalAnimator = getItemAnimator();
        setItemAnimator(null);
    }

    /**
     * Start showing the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startShowing(boolean animate) {
        assert mFadeOutAnimator == null;
        mListener.startedShowing(animate);

        long duration = TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())
                ? FINAL_FADE_IN_DURATION_MS
                : BASE_ANIMATION_DURATION_MS;

        setAlpha(0);
        setVisibility(View.VISIBLE);
        mFadeInAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 1);
        mFadeInAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mFadeInAnimator.setDuration(duration);
        mFadeInAnimator.start();
        mFadeInAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeInAnimator = null;
                mSuppressCapture = true;
                mListener.finishedShowing();
                // Restore the original value.
                // TODO(crbug.com/1315676): Remove the null check after decoupling Start surface
                // layout and grid tab switcher layout.
                if (mOriginalAnimator != null) {
                    setItemAnimator(mOriginalAnimator);
                    mOriginalAnimator = null;
                }
                setShadowVisibility(computeVerticalScrollOffset() > 0);
                if (mDynamicView != null) {
                    unregisterDynamicView();
                    mDynamicView.dropCachedBitmap();
                }
                // TODO(crbug.com/972157): remove this band-aid after we know why GTS is invisible.
                if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())) {
                    ViewUtils.requestLayout(TabListRecyclerView.this,
                            "TabListRecyclerView.startShowing.AnimatorListenerAdapter.onAnimationEnd");
                }
            }
        });
        if (!animate) mFadeInAnimator.end();
    }

    /**
     * Updates the toolbar hairline drawable color appropriately for the regular and incognito tab
     * models.
     * @param color The toolbar hairline color.
     */
    void setToolbarHairlineColor(@ColorInt int color) {
        mToolbarHairlineColor = color;
        // If the drawable is already initialized, update its color when switching between regular
        // and incognito tab models.
        if (mShadowImageView != null) {
            mShadowImageView.setImageTintList(ColorStateList.valueOf(color));
        }
    }

    void setShadowVisibility(boolean shouldShowShadow) {
        if (mShadowImageView == null) {
            Context context = getContext();
            mShadowImageView = new ImageView(context);
            Drawable drawable = context.getDrawable(R.drawable.toolbar_hairline);
            mShadowImageView.setImageDrawable(drawable);
            mShadowImageView.setScaleType(ImageView.ScaleType.FIT_XY);
            mShadowImageView.setTag(SHADOW_VIEW_TAG);
            Resources res = context.getResources();
            int shadowHeight = res.getDimensionPixelSize(R.dimen.toolbar_hairline_height);
            if (getParent() instanceof FrameLayout) {
                // Add shadow for grid tab switcher.
                FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, shadowHeight, Gravity.TOP);
                mShadowImageView.setLayoutParams(params);
                mShadowImageView.setTranslationY(mShadowTopOffset);
                FrameLayout parent = (FrameLayout) getParent();
                parent.addView(mShadowImageView);
            } else if (getParent() instanceof RelativeLayout) {
                // Add shadow for tab grid dialog.
                RelativeLayout parent = (RelativeLayout) getParent();
                View toolbar = parent.getChildAt(0);
                if (!(toolbar instanceof TabGroupUiToolbarView)) return;

                RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, shadowHeight);
                params.addRule(RelativeLayout.BELOW, toolbar.getId());
                parent.addView(mShadowImageView, params);
            }
        }

        mShadowImageView.setImageTintList(ColorStateList.valueOf(mToolbarHairlineColor));
        if (shouldShowShadow && mShadowImageView.getVisibility() != VISIBLE) {
            mShadowImageView.setVisibility(VISIBLE);
        } else if (!shouldShowShadow && mShadowImageView.getVisibility() != GONE) {
            mShadowImageView.setVisibility(GONE);
        }
    }

    void setShadowTopOffset(int shadowTopOffset) {
        mShadowTopOffset = shadowTopOffset;

        if (mShadowImageView != null && getParent() instanceof FrameLayout) {
            // Since the shadow has no functionality, other than just existing visually, we can use
            // translationY to position it using the top offset. This is preferable to setting a
            // margin because translation doesn't require a relayout.
            mShadowImageView.setTranslationY(mShadowTopOffset);

            // Set the shadow visibility using the newly computed scroll offset in case the new
            // layout requires us to toggle the shadow visibility. E.g. the height increases and the
            // grid isn't scrolled anymore.
            final int scrollOffset = computeVerticalScrollOffset();
            if (scrollOffset == 0) {
                setShadowVisibility(false);
            } else if (scrollOffset > 0) {
                setShadowVisibility(true);
            }
        }
    }

    void setBottomPadding(int bottomPadding) {
        setPadding(getPaddingLeft(), getPaddingTop(), getPaddingRight(), bottomPadding);
    }

    /**
     * @return The ID for registering and using the dynamic resource in compositor.
     */
    int getResourceId() {
        return mResourceId;
    }

    long getLastDirtyTime() {
        return mLastDirtyTime;
    }

    private float getDownsamplingScale() {
        String scale = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, DOWNSAMPLING_SCALE_PARAM);
        try {
            return Float.valueOf(scale);
        } catch (NumberFormatException e) {
            return DEFAULT_DOWNSAMPLING_SCALE;
        }
    }

    /**
     * Create a DynamicResource for this RecyclerView.
     * The view resource can be obtained by {@link #getResourceId} in compositor layer.
     */
    void createDynamicView(DynamicResourceLoader loader) {
        // TODO(crbug/1409886): Consider reducing capture frequency or only capturing once. There
        // was some discussion about this in crbug/1386265. However, it was punted on due to mid-end
        // devices having difficulty producing thumbnails before the first capture to avoid the
        // transition being jarring. This is exacerbated by multi-thumbnails which need to be
        // assembled from multiple assets.
        mDynamicView = new ViewResourceAdapter(this) {
            private long mSuppressedUntil;

            @Override
            public boolean isDirty() {
                boolean dirty = super.isDirty();
                if (dirty) {
                    mLastDirtyTime = SystemClock.elapsedRealtime();
                }
                if (SystemClock.elapsedRealtime() < mSuppressedUntil || mSuppressCapture) {
                    if (dirty) {
                        Log.d(TAG, "Dynamic View is dirty but suppressed");
                    }
                    return false;
                }
                return dirty;
            }

            @Override
            public void triggerBitmapCapture() {
                long startTime = SystemClock.elapsedRealtime();
                super.triggerBitmapCapture();
                long elapsed = SystemClock.elapsedRealtime() - startTime;
                if (elapsed == 0) elapsed = 1;

                float maxDutyCycle = getMaxDutyCycle();
                Log.d(TAG, "MaxDutyCycle = " + getMaxDutyCycle());
                assert maxDutyCycle > 0;
                assert maxDutyCycle <= 1;
                long suppressedFor = Math.min(
                        (long) (elapsed * (1 - maxDutyCycle) / maxDutyCycle), ZOOMING_DURATION);

                mSuppressedUntil = SystemClock.elapsedRealtime() + suppressedFor;
                Log.d(TAG, "DynamicView: spent %dms on getBitmap, suppress updating for %dms.",
                        elapsed, suppressedFor);
            }
        };
        mDynamicView.setDownsamplingScale(getDownsamplingScale());
        assert mLoader == null : "createDynamicView should only be called once";
        mLoader = loader;
    }

    private float getMaxDutyCycle() {
        String maxDutyCycle = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, MAX_DUTY_CYCLE_PARAM);
        try {
            return Float.valueOf(maxDutyCycle);
        } catch (NumberFormatException e) {
            return DEFAULT_MAX_DUTY_CYCLE;
        }
    }

    private void registerDynamicView() {
        if (mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.registerResource(getResourceId(), mDynamicView);
        mIsDynamicViewRegistered = true;
    }

    private void unregisterDynamicView() {
        if (!mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.unregisterResource(getResourceId());
        mIsDynamicViewRegistered = false;
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        if (mDynamicView != null) {
            mDynamicView.invalidate(null);
        }
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        ViewParent retVal = super.invalidateChildInParent(location, dirty);
        if (mDynamicView != null) {
            mDynamicView.invalidate(dirty);
        }
        return retVal;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mScrollListener = new TabListOnScrollListener();
        addOnScrollListener(mScrollListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mShadowImageView != null) {
            removeViewInLayout(mShadowImageView);
            mShadowImageView = null;
        }

        if (mScrollListener != null) {
            removeOnScrollListener(mScrollListener);
            mScrollListener = null;
        }
    }

    /**
     * Start hiding the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startHiding(boolean animate) {
        endAllAnimations();

        registerDynamicView();
        if (mDynamicView == null) {
            hideAnimation(animate);
            return;
        }
        DynamicResourceReadyOnceCallback.onNext(mDynamicView, resource -> {
            mSuppressCapture = false;
            hideAnimation(animate);
        });
        mDynamicView.triggerBitmapCapture();
    }

    private void hideAnimation(boolean animate) {
        mListener.startedHiding(animate);
        mFadeOutAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 0);
        mFadeOutAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mFadeOutAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeOutAnimator = null;
                setVisibility(View.INVISIBLE);
                mSuppressCapture = true;
                mListener.finishedHiding();
            }
        });
        setShadowVisibility(false);
        mFadeOutAnimator.start();
        if (!animate) mFadeOutAnimator.end();
    }

    void postHiding() {
        if (mDynamicView != null) {
            unregisterDynamicView();
            mDynamicView.dropCachedBitmap();
        }
    }

    private void endAllAnimations() {
        if (mFadeInAnimator != null) {
            mFadeInAnimator.end();
        }
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.end();
        }
    }

    /**
     * @param selectedTabIndex The index in the RecyclerView of the selected tab.
     * @param selectedTabId The tab ID of the selected tab.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         {@link TabListRecyclerView} coordinates.
     */
    @Nullable
    Rect getRectOfCurrentThumbnail(int selectedTabIndex, int selectedTabId) {
        SimpleRecyclerViewAdapter.ViewHolder holder =
                (SimpleRecyclerViewAdapter.ViewHolder) findViewHolderForAdapterPosition(
                        selectedTabIndex);
        if (holder == null || selectedTabIndex == TabModel.INVALID_TAB_INDEX) return null;
        assert holder.model.get(TabProperties.TAB_ID) == selectedTabId;
        ViewLookupCachingFrameLayout root = (ViewLookupCachingFrameLayout) holder.itemView;
        return getRectOfComponent(root.fastFindViewById(R.id.tab_thumbnail));
    }

    private Rect getRectOfComponent(View v) {
        // If called before a thumbnail view exists or for list view then exit with null.
        if (v == null) return null;

        Rect recyclerViewRect = new Rect();
        Rect componentRect = new Rect();
        getGlobalVisibleRect(recyclerViewRect);
        v.getGlobalVisibleRect(componentRect);

        // Get the relative position.
        componentRect.offset(-recyclerViewRect.left, -recyclerViewRect.top);
        return componentRect;
    }

    /**
     * A structure for holding the a recycler view position and offset.
     */
    public static class RecyclerViewPosition {
        private int mPosition;
        private int mOffset;

        /**
         * @param position The position of the first visible item in the recyclerView.
         * @param offset The scroll offset of the recyclerView;
         */
        public RecyclerViewPosition(int position, int offset) {
            mPosition = position;
            mOffset = offset;
        }

        /**
         * @return the position of the first visible item in the RecyclerView.
         */
        public int getPosition() {
            return mPosition;
        }

        /**
         * @return the offset from the first item in the RecyclerView.
         */
        public int getOffset() {
            return mOffset;
        }
    }

    /**
     * @return the position and offset of the first visible element in the list.
     */
    @NonNull
    RecyclerViewPosition getRecyclerViewPosition() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) getLayoutManager();
        int position = layoutManager.findFirstVisibleItemPosition();
        int offset = 0;
        if (position != RecyclerView.NO_POSITION) {
            View firstVisibleView = layoutManager.findViewByPosition(position);
            if (firstVisibleView != null) {
                offset = firstVisibleView.getTop();
            }
        }
        return new RecyclerViewPosition(position, offset);
    }

    /**
     * @param recyclerViewPosition the position and offset to scroll the recycler view to.
     */
    void setRecyclerViewPosition(@NonNull RecyclerViewPosition recyclerViewPosition) {
        ((LinearLayoutManager) getLayoutManager())
                .scrollToPositionWithOffset(
                        recyclerViewPosition.getPosition(), recyclerViewPosition.getOffset());
    }

    /**
     * This method finds out the index of the hovered tab's viewHolder in {@code recyclerView}.
     * @param recyclerView   The recyclerview that owns the tabs' viewHolders.
     * @param view           The view of the selected tab.
     * @param dX             The X offset of the selected tab.
     * @param dY             The Y offset of the selected tab.
     * @param threshold      The threshold to judge whether two tabs are overlapped.
     * @return The index of the hovered tab.
     */
    static int getHoveredTabIndex(
            RecyclerView recyclerView, View view, float dX, float dY, float threshold) {
        for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
            ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder == null) continue;
            View child = viewHolder.itemView;
            if (child.getLeft() == view.getLeft() && child.getTop() == view.getTop()) {
                continue;
            }
            if (isOverlap(child.getLeft(), child.getTop(), view.getLeft() + dX, view.getTop() + dY,
                        threshold)) {
                return i;
            }
        }
        return -1;
    }

    private static boolean isOverlap(
            float left1, float top1, float left2, float top2, float threshold) {
        return Math.abs(left1 - left2) < threshold && Math.abs(top1 - top2) < threshold;
    }

    // TabGridAccessibilityHelper implementation.
    // TODO(crbug.com/1032095): Add e2e tests for implementation below when tab grid is enabled for
    // accessibility mode.
    @Override
    @SuppressLint("NewApi")
    public List<AccessibilityAction> getPotentialActionsForView(View view) {
        List<AccessibilityAction> actions = new ArrayList<>();
        int position = getChildAdapterPosition(view);
        if (position == -1) {
            return actions;
        }
        assert getLayoutManager() instanceof GridLayoutManager;
        GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        int spanCount = layoutManager.getSpanCount();
        Context context = getContext();

        AccessibilityAction leftAction = new AccessibilityNodeInfo.AccessibilityAction(
                R.id.move_tab_left, context.getString(R.string.accessibility_tab_movement_left));
        AccessibilityAction rightAction = new AccessibilityNodeInfo.AccessibilityAction(
                R.id.move_tab_right, context.getString(R.string.accessibility_tab_movement_right));
        AccessibilityAction topAction = new AccessibilityNodeInfo.AccessibilityAction(
                R.id.move_tab_up, context.getString(R.string.accessibility_tab_movement_up));
        AccessibilityAction downAction = new AccessibilityNodeInfo.AccessibilityAction(
                R.id.move_tab_down, context.getString(R.string.accessibility_tab_movement_down));
        actions.addAll(
                new ArrayList<>(Arrays.asList(leftAction, rightAction, topAction, downAction)));

        // Decide whether the tab can be moved left/right based on current index and span count.
        if (position % spanCount == 0) {
            actions.remove(leftAction);
        } else if (position % spanCount == spanCount - 1) {
            actions.remove(rightAction);
        }
        // Cannot move up if the tab is in the first row.
        if (position < spanCount) {
            actions.remove(topAction);
        }
        // Cannot move down if current tab is the last X tab where X is the span count.
        if (getSwappableItemCount() - position <= spanCount) {
            actions.remove(downAction);
        }
        // Cannot move the last tab to its right.
        if (position == getSwappableItemCount() - 1) {
            actions.remove(rightAction);
        }
        return actions;
    }

    private int getSwappableItemCount() {
        int count = 0;
        for (int i = 0; i < getAdapter().getItemCount(); i++) {
            if (getAdapter().getItemViewType(i) == TabProperties.UiType.CLOSABLE) count++;
        }
        return count;
    }

    @Override
    public Pair<Integer, Integer> getPositionsOfReorderAction(View view, int action) {
        int currentPosition = getChildAdapterPosition(view);
        assert getLayoutManager() instanceof GridLayoutManager;
        GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        int spanCount = layoutManager.getSpanCount();
        int targetPosition = -1;

        if (action == R.id.move_tab_left) {
            targetPosition = currentPosition - 1;
        } else if (action == R.id.move_tab_right) {
            targetPosition = currentPosition + 1;
        } else if (action == R.id.move_tab_up) {
            targetPosition = currentPosition - spanCount;
        } else if (action == R.id.move_tab_down) {
            targetPosition = currentPosition + spanCount;
        }
        return new Pair<>(currentPosition, targetPosition);
    }

    @Override
    public boolean isReorderAction(int action) {
        return action == R.id.move_tab_left || action == R.id.move_tab_right
                || action == R.id.move_tab_up || action == R.id.move_tab_down;
    }

    ImageView getShadowImageViewForTesting() {
        return mShadowImageView;
    }

    int getToolbarHairlineColorForTesting() {
        return mToolbarHairlineColor;
    }
}
