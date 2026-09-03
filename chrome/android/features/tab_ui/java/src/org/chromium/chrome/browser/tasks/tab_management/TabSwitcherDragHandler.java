// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PointF;
import android.util.FloatProperty;
import android.view.DragEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.interpolators.Interpolators;

import java.util.function.Supplier;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * TabListCoordinator}.
 */
@NullMarked
public class TabSwitcherDragHandler extends TabDragHandlerBase {
    static final long DRAG_SHADOW_ANIMATION_DURATION_MS = 200L;

    /** Allows to handle tab drag and drop events. */
    public interface DragHandlerDelegate {
        default boolean handleDragStart(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragStart(View view, float xPx, float yPx) {
            return handleDragStart(xPx, yPx);
        }

        default boolean handleExternalDragEnd(float xPx, float yPx, boolean isOSNewWindowDrop) {
            return false;
        }

        default boolean handleExternalDragEnd(
                View view, float xPx, float yPx, boolean isOSNewWindowDrop) {
            return handleExternalDragEnd(xPx, yPx, isOSNewWindowDrop);
        }

        default boolean handleDragEnter() {
            return false;
        }

        default boolean handleDragEnter(View view) {
            return handleDragEnter();
        }

        default boolean handleDragExit() {
            return false;
        }

        default boolean handleDragExit(View view) {
            return handleDragExit();
        }

        default boolean handleDragLocation(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragLocation(View view, float xPx, float yPx) {
            return handleDragLocation(xPx, yPx);
        }

        default boolean handleDrop(float xPx, float yPx) {
            return true;
        }

        default boolean handleDrop(View view, float xPx, float yPx) {
            return handleDrop(xPx, yPx);
        }

        /**
         * Returns whether a drag operation is currently in progress.
         *
         * @return True if a drag is active, false otherwise.
         */
        default boolean isDragInProcess() {
            return false;
        }

        /**
         * Handles the internal drag end event.
         *
         * @return The result of the back press handling, typically {@link BackPressResult#SUCCESS}
         *     if the drag was successfully cancelled.
         */
        default int handleInternalDragEnd() {
            return BackPressResult.FAILURE;
        }
    }

    private @Nullable DragHandlerDelegate mDragHandlerDelegate;
    private @Nullable ImageView mShadowView;
    private @Nullable AnimatedDragShadowBuilder mCurrentDragShadowBuilder;
    private final TabSwitcherBackPressHandlerManager mDragHandlerManager;
    private final boolean mFadeDragShadow;

    /**
     * Tracks whether this specific drag handler instance processed {@link DragEvent#ACTION_DROP}.
     * Used on {@link DragEvent#ACTION_DRAG_ENDED} to distinguish a drop handled internally within
     * this tab container from an external drop (handled by another tab container, another Chrome
     * window, or an OS new-window drop).
     */
    private boolean mDropHandledInCurrentHandler;

    /**
     * Prepares the tab container view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param activitySupplier Supplier for the current activity.
     * @param multiInstanceManager {@link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     * @param dragHandlerManager Manager for back press handling during drag.
     * @param fadeDragShadow Whether the drag shadow should animate alpha during drag.
     */
    public TabSwitcherDragHandler(
            Supplier<@Nullable Activity> activitySupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            TabSwitcherBackPressHandlerManager dragHandlerManager,
            boolean fadeDragShadow) {
        super(activitySupplier, multiInstanceManager, dragAndDropDelegate);
        mDragHandlerManager = dragHandlerManager;
        mDragHandlerManager.addHandler(this);
        mFadeDragShadow = fadeDragShadow;
    }

    public void onDragStateChanged(boolean isDragInProcess) {
        if (isDragInProcess) {
            onInternalDragStarted();
        } else {
            onInternalDragEnded();
        }
    }

    @Override
    public Boolean handleEscPress() {
        assumeNonNull(mDragHandlerDelegate);
        if (mDragHandlerDelegate.isDragInProcess()) {
            return mDragHandlerDelegate.handleInternalDragEnd() == BackPressResult.SUCCESS;
        }
        return super.handleEscPress();
    }

    /**
     * Sets an object to handle tab drag events.
     *
     * @param dragHandlerDelegate Instance of {@link DragHandlerDelegate}
     */
    public void setDragHandlerDelegate(DragHandlerDelegate dragHandlerDelegate) {
        mDragHandlerDelegate = dragHandlerDelegate;
    }

    /**
     * Starts the tab drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tab Tab is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     * @param dragShadowView The view used to generate the drag shadow.
     * @return true if the drag action was initiated successfully.
     */
    public boolean startTabDragAction(
            View dragSourceView, Tab tab, PointF startPoint, @Nullable View dragShadowView) {
        if (!canStartTabDrag()) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareTabDropData(tab);
        return startDragInternal(dropData, startPoint, dragSourceView, dragShadowView);
    }

    /**
     * Starts the group drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabGroupId The dragged group's ID.
     * @param startPoint Position of the drag start point in view coordinates.
     * @param dragShadowView The view used to generate the drag shadow (optional).
     * @return {@code True} if the drag action was initiated successfully.
     */
    public boolean startGroupDragAction(
            View dragSourceView,
            Token tabGroupId,
            PointF startPoint,
            @Nullable View dragShadowView) {
        if (!canStartGroupDrag(tabGroupId)) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareGroupDropData(tabGroupId, false);
        return startDragInternal(dropData, startPoint, dragSourceView, dragShadowView);
    }

    /**
     * Toggles visibility of the external drag shadow.
     *
     * @param show True to display the drag shadow, false to hide it (draw empty/transparent).
     */
    public void showDragShadow(boolean show) {
        showDragShadow(null, show);
    }

    /**
     * Toggles visibility of the external drag shadow using an attached view to ensure OS delivery.
     *
     * @param attachedView An attached View in the current window to dispatch the update through.
     * @param show True to display the drag shadow, false to hide it (draw empty/transparent).
     */
    public void showDragShadow(@Nullable View attachedView, boolean show) {
        View.DragShadowBuilder builder = mCurrentDragShadowBuilder;
        if (builder == null) {
            builder = DragDropGlobalState.getDragShadowBuilder();
        }
        if (builder instanceof AnimatedDragShadowBuilder animatedBuilder) {
            animatedBuilder.update(attachedView, show);
        }
    }

    /** Returns whether this handler currently has an active drag shadow builder. */
    public boolean hasActiveDragShadow() {
        return mCurrentDragShadowBuilder != null;
    }

    /**
     * Refreshes the drag shadow with the updated contents of the custom drag shadow view.
     *
     * @param dragShadowView The custom drag shadow view with updated contents.
     */
    public void refreshDragShadow(@Nullable View dragShadowView) {
        AnimatedDragShadowBuilder shadowBuilder = mCurrentDragShadowBuilder;
        View dragSourceView = mDragSourceView;
        if (shadowBuilder == null || dragShadowView == null || dragSourceView == null) return;
        updateShadowView(dragSourceView, dragShadowView);
        shadowBuilder.updateDragShadow(dragSourceView);
    }

    private boolean startDragInternal(
            ChromeDropDataAndroid dropData,
            PointF startPoint,
            View dragSourceView,
            @Nullable View dragShadowView) {
        updateShadowView(dragSourceView, dragShadowView);
        assert mShadowView != null;

        // TODO(crbug.com/425901698): consider using {@link AnimatedImageDragShadowBuilder}.
        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        dragSourceView,
                        mShadowView,
                        startPoint,
                        DRAG_SHADOW_ANIMATION_DURATION_MS,
                        mFadeDragShadow);
        mCurrentDragShadowBuilder = builder;

        // Hide the item before trying to start drag. Hiding it at the ItemTouchHelper2 is too late
        // and might visually produce two overlapping items (the original item and the drag shadow).
        dragSourceView.setAlpha(0);
        boolean dragStarted = startDrag(dragSourceView, builder, dropData);
        if (!dragStarted) {
            // Restore items's visibility if unable to start drag.
            dragSourceView.setAlpha(1);
            mCurrentDragShadowBuilder = null;
        }
        return dragStarted;
    }

    private void updateShadowView(View dragSourceView, @Nullable View dragShadowView) {
        initShadowView(dragSourceView);
        assert mShadowView != null;

        // Capture the original view's drawing into a bitmap.
        View snapshotView = viewToSnapshot(dragSourceView, dragShadowView);
        int width = snapshotView.getWidth();
        int height = snapshotView.getHeight();
        if (width <= 0 || height <= 0) {
            width = dragSourceView.getWidth();
            height = dragSourceView.getHeight();
        }
        Bitmap canvasBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(canvasBitmap);
        snapshotView.draw(canvas);

        // Update dragShadowView with the captured bitmap.
        mShadowView.layout(0, 0, width, height);
        mShadowView.setImageBitmap(canvasBitmap);
    }

    private View viewToSnapshot(View dragSourceView, @Nullable View dragShadowView) {
        return dragShadowView != null ? dragShadowView : dragSourceView;
    }

    private void initShadowView(View dragSourceView) {
        if (mShadowView != null) return;

        View rootView =
                View.inflate(
                        dragSourceView.getContext(),
                        R.layout.tab_switcher_drag_shadow_view,
                        (ViewGroup) dragSourceView.getRootView());
        mShadowView = rootView.findViewById(R.id.tab_switcher_drag_shadow_view);
    }

    private void destroyShadowView() {
        if (mShadowView == null) return;

        ViewGroup parent = (ViewGroup) mShadowView.getParent();
        if (parent != null) {
            parent.removeView(mShadowView);
        }
        mShadowView = null;
    }

    @Override
    public void destroy() {
        mDragHandlerManager.removeHandler(this);
        super.destroy();
        destroyShadowView();
        mCurrentDragShadowBuilder = null;
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean res = false;

        // No-op if the handler delegate is missing.
        if (mDragHandlerDelegate == null) {
            return res;
        }

        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                mDropHandledInCurrentHandler = false;
                if (isDraggingBrowserContent(dragEvent.getClipDescription())) {
                    if (!doesBelongToCurrentModel(isDraggedItemIncognito())) {
                        return false;
                    }
                    res =
                            mDragHandlerDelegate.handleDragStart(
                                    view, dragEvent.getX(), dragEvent.getY());
                }
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                // TODO(crbug.com/518307037): Use a TabModelObserver.
                boolean isExternalDrop =
                        dragEvent.getResult()
                                && DragDropGlobalState.hasValue()
                                && !mDropHandledInCurrentHandler;
                // Restore items's visibility.
                if (mDragSourceView != null) {
                    if (!isExternalDrop) {
                        mDragSourceView.setAlpha(1);
                    }
                    finishDrag(dragEvent.getResult());
                } else {
                    view.setAlpha(1);
                }
                res =
                        mDragHandlerDelegate.handleExternalDragEnd(
                                view, dragEvent.getX(), dragEvent.getY(), isExternalDrop);
                mCurrentDragShadowBuilder = null;
                mDropHandledInCurrentHandler = false;
                break;
            case DragEvent.ACTION_DRAG_ENTERED:
                if (!doesBelongToCurrentModel(isDraggedItemIncognito())) {
                    return false;
                }
                res = mDragHandlerDelegate.handleDragEnter(view);
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                res = mDragHandlerDelegate.handleDragExit(view);
                break;
            case DragEvent.ACTION_DRAG_LOCATION:
                if (!doesBelongToCurrentModel(isDraggedItemIncognito())) {
                    return false;
                }
                res =
                        mDragHandlerDelegate.handleDragLocation(
                                view, dragEvent.getX(), dragEvent.getY());
                break;
            case DragEvent.ACTION_DROP:
                if (!doesBelongToCurrentModel(isDraggedItemIncognito())) {
                    return false;
                }
                res = mDragHandlerDelegate.handleDrop(view, dragEvent.getX(), dragEvent.getY());
                if (res) {
                    mDropHandledInCurrentHandler = true;
                    DragDropGlobalState.notifyChromeHandledDrop(dragEvent);
                }
                break;
        }
        return res;
    }

    static class AnimatedDragShadowBuilder extends View.DragShadowBuilder {

        private static final FloatProperty<AnimatedDragShadowBuilder> PROGRESS =
                new FloatProperty<>("progress") {
                    @Override
                    public void setValue(AnimatedDragShadowBuilder object, float v) {
                        object.setProgress(v);
                    }

                    @Override
                    public Float get(AnimatedDragShadowBuilder object) {
                        return object.getProgress();
                    }
                };

        private final View mOriginalView;
        private final PointF mTouchPointF;
        private final long mAnimationDuration;
        private final float mStartWidth;
        private final float mStartHeight;
        private final boolean mFadeDragShadow;

        private float mProgress;
        private boolean mShowDragShadow = true;

        public AnimatedDragShadowBuilder(
                View view,
                View dragShadowView,
                PointF startPointF,
                long animationDuration,
                boolean fadeDragShadow) {
            super(dragShadowView);
            mOriginalView = view;
            mAnimationDuration = animationDuration;
            mStartWidth = dragShadowView.getWidth();
            mStartHeight = dragShadowView.getHeight();
            mFadeDragShadow = fadeDragShadow;

            if (dragShadowView != mOriginalView) {
                // If using a custom shadow representing a grid card, mimic horizontal tab strip
                // logic
                Resources resources = dragShadowView.getContext().getResources();
                float headerHeight = resources.getDimension(R.dimen.tab_grid_card_header_height);
                float cardMargin = resources.getDimension(R.dimen.tab_grid_card_margin);

                // Horizontally center with the cursor
                float dragShadowOffsetX = mStartWidth / 2;

                // Vertically center in the tab title header
                float dragShadowOffsetY = (headerHeight / 2) + cardMargin;

                mTouchPointF = new PointF(dragShadowOffsetX, dragShadowOffsetY);
            } else {
                float relativeX = (startPointF.x - mOriginalView.getX()) / mOriginalView.getWidth();
                float relativeY =
                        (startPointF.y - mOriginalView.getY()) / mOriginalView.getHeight();
                mTouchPointF = new PointF(mStartWidth * relativeX, mStartHeight * relativeY);
            }

            if (mFadeDragShadow) {
                dragShadowView.post(this::animate);
            }
        }

        /**
         * Updates the drag shadow visibility.
         *
         * @param show True to display the drag shadow, false to hide it.
         */
        public void update(boolean show) {
            update(null, show);
        }

        /**
         * Updates the drag shadow visibility using an attached view to ensure OS delivery.
         *
         * @param attachedView An attached View in the current window to dispatch the update
         *     through.
         * @param show True to display the drag shadow, false to hide it.
         */
        public void update(@Nullable View attachedView, boolean show) {
            if (mShowDragShadow == show) return;
            mShowDragShadow = show;
            updateDragShadow(attachedView);
        }

        private void animate() {
            ObjectAnimator updateAnimator = ObjectAnimator.ofFloat(this, PROGRESS, 1f, 0.8f);
            updateAnimator.setDuration(mAnimationDuration);
            updateAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
            updateAnimator.start();
        }

        private void setProgress(float progress) {
            assert progress >= 0.f && progress <= 1.f : "Invalid animation progress value.";
            mProgress = progress;
            View view = getView();
            if (view != null) {
                // Apply scaled measurements to the LayoutParams.
                ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
                layoutParams.width = (int) (mStartWidth * progress);
                layoutParams.height = (int) (mStartHeight * progress);
                view.setLayoutParams(layoutParams);
                view.post(this::update);
            }
        }

        private float getProgress() {
            return mProgress;
        }

        private void update() {
            updateDragShadow(null);
        }

        private void updateDragShadow(@Nullable View attachedView) {
            View viewToUse = null;
            View originalRoot = mOriginalView != null ? mOriginalView.getRootView() : null;
            View shadowView = getView();
            View shadowRoot = shadowView != null ? shadowView.getRootView() : null;

            if (attachedView != null && attachedView.isAttachedToWindow()) {
                viewToUse = attachedView;
            } else if (mOriginalView != null && mOriginalView.isAttachedToWindow()) {
                viewToUse = mOriginalView;
            } else if (originalRoot != null && originalRoot.isAttachedToWindow()) {
                viewToUse = originalRoot;
            } else if (shadowView != null && shadowView.isAttachedToWindow()) {
                viewToUse = shadowView;
            } else if (shadowRoot != null && shadowRoot.isAttachedToWindow()) {
                viewToUse = shadowRoot;
            }

            if (viewToUse != null) {
                viewToUse.updateDragShadow(this);
            }
        }

        @Override
        public void onProvideShadowMetrics(Point shadowSize, Point shadowTouchPoint) {
            View view = getView();
            if (view != null && mShowDragShadow) {
                shadowSize.set(view.getWidth(), view.getHeight());
                shadowTouchPoint.set((int) mTouchPointF.x, (int) mTouchPointF.y);
            } else {
                shadowSize.set(1, 1);
                shadowTouchPoint.set(0, 0);
            }
        }

        @Override
        public void onDrawShadow(Canvas canvas) {
            if (!mShowDragShadow) {
                return;
            }
            View view = getView();
            if (view != null) {
                if (!mFadeDragShadow) {
                    view.draw(canvas);
                    return;
                }
                float progress = getProgress();
                // Apply alpha value.
                Paint paint = new Paint();
                paint.setAntiAlias(true);
                paint.setAlpha((int) (255 * progress));
                // Apply translation to keep the scaled drag shadow under the initial touch point.
                float touchPointCorrection = 1 - progress;
                float translateX = mTouchPointF.x * touchPointCorrection;
                float translateY = mTouchPointF.y * touchPointCorrection;
                int layerId = canvas.saveLayer(0, 0, canvas.getWidth(), canvas.getHeight(), paint);
                canvas.translate(translateX, translateY);
                view.draw(canvas);
                canvas.restoreToCount(layerId);
            }
        }
    }
}
