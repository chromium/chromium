// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.constraintlayout.widget.ConstraintLayout;

import com.google.android.material.progressindicator.CircularProgressIndicator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;

/**
 * Root container layout for a single vertical tab row item.
 *
 * <p>Owns the child layout manipulation for a tab row: switching between compact icon-centered mode
 * and expanded row mode, expanding the action button touch target, and driving the actuation
 * spinner animation. Also caches the lookups for the children that {@link TabVerticalViewBinder}
 * resolves on every bind.
 */
@NullMarked
public class VerticalTabItemLayout extends ConstraintLayout {
    private static final float ACTUATION_SPINNER_ROTATION_DEGREES = 360f;
    private static final long ACTUATION_SPINNER_DURATION_MS = 2000L;

    private @Nullable TextView mTitleView;
    private @Nullable ImageView mActionButton;

    public VerticalTabItemLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.tab_title);
        mActionButton = findViewById(R.id.action_button);
    }

    @Nullable TextView getTitleView() {
        return mTitleView;
    }

    @Nullable ImageView getActionButton() {
        return mActionButton;
    }

    /**
     * Updates constraints, visibility, colors, touch delegate, and animations for the row's icon
     * views.
     *
     * <p>TODO(crbug.com/542280452): Once pinned tab rows inflate {@code vertical_tab_item.xml} as
     * well, make this an instance method that reads the cached child views instead of resolving
     * them per call, and drop the {@code root} parameter.
     *
     * @param root The row container. Children are resolved by id so that rows which are not yet a
     *     {@link VerticalTabItemLayout} (currently only pinned tab rows) keep working.
     * @param isCompact Whether the row is in compact mode.
     * @param showActionButton Whether the action button should be visible.
     * @param showAlertIndicator Whether the alert indicator should be visible.
     * @param isDynamicActorAlert Whether the actuation spinner animation should run.
     * @param showLoading Whether the loading spinner should be visible.
     * @param loadingSpinnerColor The color tint for the loading spinner.
     * @param showFavicon Whether the website favicon should be visible.
     */
    static void updateIconDisplay(
            ViewGroup root,
            boolean isCompact,
            boolean showActionButton,
            boolean showAlertIndicator,
            boolean isDynamicActorAlert,
            boolean showLoading,
            @ColorInt int loadingSpinnerColor,
            boolean showFavicon) {
        View actionButton = root.findViewById(R.id.action_button);
        if (actionButton != null) {
            updateChildConstraints(
                    actionButton,
                    isCompact,
                    LayoutParams.UNSET,
                    LayoutParams.UNSET,
                    LayoutParams.PARENT_ID,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ 0);
            actionButton.setVisibility(showActionButton ? View.VISIBLE : View.GONE);
            updateActionButtonTouchDelegate(root, actionButton, showActionButton);
        }

        ImageView alertIndicator = root.findViewById(R.id.alert_indicator_icon);
        if (alertIndicator != null) {
            updateChildConstraints(
                    alertIndicator,
                    isCompact,
                    LayoutParams.UNSET,
                    R.id.action_button,
                    LayoutParams.UNSET,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ R.dimen.vertical_tab_item_alert_indicator_margin_end);
            alertIndicator.setVisibility(showAlertIndicator ? View.VISIBLE : View.GONE);

            ImageView actuationSpinner = root.findViewById(R.id.actuation_spinner);
            if (actuationSpinner != null) {
                updateActorSpinnerAnimation(actuationSpinner, isDynamicActorAlert);
            }
        }

        View faviconContainer = root.findViewById(R.id.favicon_container);
        if (faviconContainer != null && (showLoading || showFavicon)) {
            updateChildConstraints(
                    faviconContainer,
                    isCompact,
                    LayoutParams.PARENT_ID,
                    LayoutParams.UNSET,
                    LayoutParams.UNSET,
                    /* marginStartDimenId= */ R.dimen.vertical_tab_item_padding_horizontal,
                    /* marginEndDimenId= */ 0);
        }

        CircularProgressIndicator loadingSpinner = root.findViewById(R.id.tab_loading_spinner);
        if (loadingSpinner != null) {
            if (showLoading) {
                loadingSpinner.setIndicatorColor(loadingSpinnerColor);
                loadingSpinner.show();
            } else {
                loadingSpinner.setVisibility(View.GONE);
            }
        }

        ImageView faviconView = root.findViewById(R.id.tab_favicon);
        if (faviconView != null) {
            faviconView.setVisibility(showFavicon ? View.VISIBLE : View.GONE);
        }
    }

    /** Expands the touch target of the action button via a {@link TouchDelegate}. */
    private static void updateActionButtonTouchDelegate(
            ViewGroup root, View actionButton, boolean actionWanted) {
        if (!actionWanted) {
            root.setTouchDelegate(null);
            return;
        }

        root.post(
                () -> {
                    if (!actionButton.isAttachedToWindow()
                            || actionButton.getVisibility() != View.VISIBLE) {
                        root.setTouchDelegate(null);
                        return;
                    }

                    Rect rect = new Rect();
                    actionButton.getHitRect(rect);
                    Resources res = root.getResources();
                    boolean isTablet = VerticalTabUtils.isTablet(root.getContext());
                    @DimenRes
                    int widthRes =
                            isTablet
                                    ? R.dimen.vertical_tab_action_button_touch_target_width_tablet
                                    : R.dimen.vertical_tab_action_button_touch_target_size;
                    @DimenRes
                    int heightRes =
                            isTablet
                                    ? R.dimen.vertical_tab_action_button_touch_target_height_tablet
                                    : R.dimen.vertical_tab_action_button_touch_target_size;
                    int minTouchTargetWidthPx = res.getDimensionPixelSize(widthRes);
                    int minTouchTargetHeightPx = res.getDimensionPixelSize(heightRes);

                    if (rect.width() < minTouchTargetWidthPx) {
                        int deltaX = (minTouchTargetWidthPx - rect.width()) / 2;
                        rect.left -= deltaX;
                        rect.right += deltaX;
                    }
                    if (rect.height() < minTouchTargetHeightPx) {
                        int deltaY = (minTouchTargetHeightPx - rect.height()) / 2;
                        rect.top -= deltaY;
                        rect.bottom += deltaY;
                    }
                    root.setTouchDelegate(new TouchDelegate(rect, actionButton));
                });
    }

    /** Starts or stops the infinite rotation animation of the actuation spinner. */
    private static void updateActorSpinnerAnimation(
            ImageView actuationSpinner, boolean isDynamicActorAlert) {
        ObjectAnimator animator = (ObjectAnimator) actuationSpinner.getTag(R.id.actuation_spinner);

        if (isDynamicActorAlert) {
            actuationSpinner.setVisibility(View.VISIBLE);

            if (animator == null) {
                animator =
                        ObjectAnimator.ofFloat(
                                actuationSpinner,
                                View.ROTATION,
                                0f,
                                ACTUATION_SPINNER_ROTATION_DEGREES);
                animator.setDuration(ACTUATION_SPINNER_DURATION_MS);
                animator.setRepeatCount(ObjectAnimator.INFINITE);
                animator.setInterpolator(new LinearInterpolator());
                actuationSpinner.setTag(R.id.actuation_spinner, animator);

                // Cancel the animator when the view is recycled to prevent infinite background
                // execution and memory leaks.
                ViewUtils.cancelAnimatorOnDetach(actuationSpinner, R.id.actuation_spinner);
            }
            if (!animator.isRunning()) {
                animator.start();
            }
        } else {
            if (animator != null && animator.isRunning()) {
                animator.cancel();
            }
            actuationSpinner.setVisibility(View.GONE);
        }
    }

    private static void updateChildConstraints(
            View view,
            boolean isCompact,
            int startToStart,
            int endToStart,
            int endToEnd,
            int marginStartDimenId,
            int marginEndDimenId) {
        if (isCompact) {
            applyConstraints(
                    view,
                    LayoutParams.PARENT_ID,
                    LayoutParams.UNSET,
                    LayoutParams.PARENT_ID,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ 0);
        } else {
            applyConstraints(
                    view, startToStart, endToStart, endToEnd, marginStartDimenId, marginEndDimenId);
        }
    }

    private static void applyConstraints(
            View view,
            int startToStart,
            int endToStart,
            int endToEnd,
            int marginStartDimenId,
            int marginEndDimenId) {
        if (view.getLayoutParams() instanceof LayoutParams params) {
            Resources resources = view.getResources();
            int marginStart =
                    marginStartDimenId != 0
                            ? resources.getDimensionPixelSize(marginStartDimenId)
                            : 0;
            int marginEnd =
                    marginEndDimenId != 0 ? resources.getDimensionPixelSize(marginEndDimenId) : 0;

            if (params.startToStart == startToStart
                    && params.endToStart == endToStart
                    && params.endToEnd == endToEnd
                    && params.getMarginStart() == marginStart
                    && params.getMarginEnd() == marginEnd) {
                return;
            }

            params.startToStart = startToStart;
            params.startToEnd = LayoutParams.UNSET;
            params.endToStart = endToStart;
            params.endToEnd = endToEnd;
            params.setMarginStart(marginStart);
            params.setMarginEnd(marginEnd);
            view.setLayoutParams(params);
        }
    }
}
