// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PointF;
import android.util.FloatProperty;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
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
    interface DragHandlerDelegate {
        default boolean handleDragStart(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragEnd(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragEnter() {
            return false;
        }

        default boolean handleDragExit() {
            return false;
        }

        default boolean handleDragLocation(float xPx, float yPx) {
            return false;
        }

        default boolean handleDrop(float xPx, float yPx) {
            return false;
        }
    }

    private @Nullable DragHandlerDelegate mDragHandlerDelegate;
    private @Nullable ImageView mShadowView;

    /**
     * Prepares the tab container view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param activitySupplier Supplier for the current activity.
     * @param multiInstanceManager {@link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     */
    public TabSwitcherDragHandler(
            Supplier<@Nullable Activity> activitySupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            Supplier<Boolean> isAppInDesktopWindowSupplier) {
        super(
                activitySupplier,
                multiInstanceManager,
                dragAndDropDelegate,
                isAppInDesktopWindowSupplier);
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
     * @return true if the drag action was initiated successfully.
     */
    public boolean startTabDragAction(View dragSourceView, Tab tab, PointF startPoint) {
        if (!canStartTabDrag()) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareTabDropData(tab);
        return startDragInternal(dropData, startPoint, dragSourceView);
    }

    /**
     * Starts the group drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabGroupId The dragged group's ID.
     * @param startPoint Position of the drag start point in view coordinates.
     * @return {@code True} if the drag action was initiated successfully.
     */
    public boolean startGroupDragAction(View dragSourceView, Token tabGroupId, PointF startPoint) {
        if (!canStartGroupDrag(tabGroupId)) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareGroupDropData(tabGroupId, false);
        return startDragInternal(dropData, startPoint, dragSourceView);
    }

    private boolean startDragInternal(
            ChromeDropDataAndroid dropData, PointF startPoint, View dragSourceView) {
        updateShadowView(dragSourceView);
        assert mShadowView != null;

        // TODO(crbug.com/425901698): consider using {@link AnimatedImageDragShadowBuilder}.
        DragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        dragSourceView, mShadowView, startPoint, DRAG_SHADOW_ANIMATION_DURATION_MS);

        // Hide the item before trying to start drag. Hiding it at the ItemTouchHelper2 is too late
        // and might visually produce two overlapping items (the original item and the drag shadow).
        dragSourceView.setAlpha(0);
        boolean dragStarted = startDrag(dragSourceView, builder, dropData);
        if (!dragStarted) {
            // Restore items's visibility if unable to start drag.
            dragSourceView.setAlpha(1);
        }
        return dragStarted;
    }

    private void updateShadowView(View dragSourceView) {
        initShadowView(dragSourceView);
        assert mShadowView != null;

        // Capture the original view's drawing into a bitmap.
        int width = dragSourceView.getWidth();
        int height = dragSourceView.getHeight();
        Bitmap canvasBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(canvasBitmap);
        dragSourceView.draw(canvas);

        // Update dragShadowView with the captured bitmap.
        mShadowView.layout(0, 0, width, height);
        mShadowView.setImageBitmap(canvasBitmap);
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
        super.destroy();
        destroyShadowView();
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
                if (isDraggingBrowserContent(dragEvent.getClipDescription())) {
                    res = mDragHandlerDelegate.handleDragStart(dragEvent.getX(), dragEvent.getY());
                }
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                // Restore items's visibility.
                view.setAlpha(1);
                finishDrag(dragEvent.getResult());
                res = mDragHandlerDelegate.handleDragEnd(dragEvent.getX(), dragEvent.getY());
                break;
            case DragEvent.ACTION_DRAG_ENTERED:
                res = mDragHandlerDelegate.handleDragEnter();
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                res = mDragHandlerDelegate.handleDragExit();
                break;
            case DragEvent.ACTION_DRAG_LOCATION:
                res = mDragHandlerDelegate.handleDragLocation(dragEvent.getX(), dragEvent.getY());
                break;
            case DragEvent.ACTION_DROP:
                res = mDragHandlerDelegate.handleDrop(dragEvent.getX(), dragEvent.getY());
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
        private float mProgress;

        public AnimatedDragShadowBuilder(
                View view, View dragShadowView, PointF starPointF, long animationDuration) {
            super(dragShadowView);
            mOriginalView = view;
            mAnimationDuration = animationDuration;
            mTouchPointF =
                    new PointF(
                            starPointF.x - mOriginalView.getX(),
                            starPointF.y - mOriginalView.getY());
            dragShadowView.post(this::animate);
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
                layoutParams.width = (int) (mOriginalView.getWidth() * progress);
                layoutParams.height = (int) (mOriginalView.getHeight() * progress);
                view.setLayoutParams(layoutParams);
                view.post(this::update);
            }
        }

        private float getProgress() {
            return mProgress;
        }

        private void update() {
            mOriginalView.updateDragShadow(this);
        }

        @Override
        public void onProvideShadowMetrics(Point shadowSize, Point shadowTouchPoint) {
            View view = getView();
            if (view != null) {
                shadowSize.set(view.getWidth(), view.getHeight());
                shadowTouchPoint.set((int) mTouchPointF.x, (int) mTouchPointF.y);
            } else {
                shadowSize.set(1, 1);
                shadowTouchPoint.set(0, 0);
            }
        }

        @Override
        public void onDrawShadow(Canvas canvas) {
            View view = getView();
            if (view != null) {
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
