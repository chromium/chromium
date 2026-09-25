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

    private TextView mTitleView;
    private ImageView mActionButton;
    private View mAiIndicator;
    private View mFaviconContainer;
    private ImageView mFaviconView;
    private ImageView mAlertIndicator;
    private ImageView mActuationSpinner;
    private CircularProgressIndicator mLoadingSpinner;
    private boolean mIsPinned;

    public VerticalTabItemLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.tab_title);
        assert mTitleView != null;

        mActionButton = findViewById(R.id.action_button);
        assert mActionButton != null;

        mAiIndicator = findViewById(R.id.ai_indicator);
        assert mAiIndicator != null;

        mFaviconContainer = findViewById(R.id.favicon_container);
        assert mFaviconContainer != null;

        mFaviconView = findViewById(R.id.tab_favicon);
        assert mFaviconView != null;

        mAlertIndicator = findViewById(R.id.alert_indicator_icon);
        assert mAlertIndicator != null;

        mActuationSpinner = findViewById(R.id.actuation_spinner);
        assert mActuationSpinner != null;

        mLoadingSpinner = findViewById(R.id.tab_loading_spinner);
        assert mLoadingSpinner != null;
    }

    TextView getTitleView() {
        return mTitleView;
    }

    ImageView getActionButton() {
        return mActionButton;
    }

    /**
     * Updates constraints, visibility, colors, touch delegate, and animations for the row's icon
     * views. Icons priority when rail is collapsed: action > tab alert > loading > favicon
     *
     * @param isCompact Whether the row is in compact mode.
     * @param showActionButton Whether the action button should be visible.
     * @param showAlertIndicator Whether the alert indicator should be visible.
     * @param isDynamicActorAlert Whether the actuation spinner animation should run.
     * @param showLoading Whether the loading spinner should be visible.
     * @param loadingSpinnerColor The color tint for the loading spinner.
     * @param showFavicon Whether the website favicon should be visible.
     */
    void updateIconDisplay(
            boolean isCompact,
            boolean showActionButton,
            boolean showAlertIndicator,
            boolean isDynamicActorAlert,
            boolean showLoading,
            @ColorInt int loadingSpinnerColor,
            boolean showFavicon) {
        // Action Button
        updateChildConstraints(
                mActionButton,
                isCompact,
                LayoutParams.UNSET,
                LayoutParams.UNSET,
                LayoutParams.PARENT_ID,
                /* marginStartDimenId= */ 0,
                /* marginEndDimenId= */ 0);
        mActionButton.setVisibility(showActionButton ? View.VISIBLE : View.GONE);
        updateActionButtonTouchDelegate(showActionButton);

        // Tab Alert Indicator and Actuation Spinner
        updateChildConstraints(
                mAlertIndicator,
                isCompact,
                LayoutParams.UNSET,
                R.id.action_button,
                LayoutParams.UNSET,
                /* marginStartDimenId= */ 0,
                /* marginEndDimenId= */ R.dimen.vertical_tab_item_alert_indicator_margin_end);
        mAlertIndicator.setVisibility(showAlertIndicator ? View.VISIBLE : View.GONE);
        updateActorSpinnerAnimation(mActuationSpinner, isDynamicActorAlert);

        // Favicon container constraints (loading spinner or tab favicon)
        if (showLoading || showFavicon) {
            updateChildConstraints(
                    mFaviconContainer,
                    isCompact,
                    LayoutParams.PARENT_ID,
                    LayoutParams.UNSET,
                    LayoutParams.UNSET,
                    /* marginStartDimenId= */ R.dimen.vertical_tab_item_padding_horizontal,
                    /* marginEndDimenId= */ 0);
        }

        // Loading Spinner
        if (showLoading) {
            mLoadingSpinner.setIndicatorColor(loadingSpinnerColor);
            mLoadingSpinner.show();
        } else {
            mLoadingSpinner.setVisibility(View.GONE);
        }

        // Favicon
        mFaviconView.setVisibility(showFavicon ? View.VISIBLE : View.GONE);
    }

    boolean isPinned() {
        return mIsPinned;
    }

    /**
     * Configures this item for pinned tab display, reproducing the layout that pinned rows had
     * before they shared {@code vertical_tab_item.xml}: the pinned background, pinned height and
     * bottom margin, no row padding, no title or close button, an inset AI indicator, and a
     * centered favicon.
     *
     * <p>This is one-way: a configured row can never go back to standard tab display. Call it once,
     * when the pinned row's view is created. That is safe because pinned rows have their own view
     * type and their own {@code RecyclerView}, so they are never recycled into the standard tab
     * list.
     *
     * <p>{@link TabVerticalViewBinder} independently derives pinned display from the model, so it
     * also hides the close button and keeps the favicon centered on every bind. This method only
     * has to cover the state before the first bind, and the properties the binder never touches
     * (background, padding, title, AI indicator margin).
     */
    void configureAsPinnedTab() {
        if (mIsPinned) return;
        mIsPinned = true;

        setBackgroundResource(R.drawable.vertical_tab_pinned_item_background);
        setPadding(0, 0, 0, 0);
        ViewGroup.LayoutParams rootParams = getLayoutParams();
        if (rootParams != null) {
            rootParams.height = TabVerticalViewBinder.getPinnedItemHeight(getContext());
            if (rootParams instanceof ViewGroup.MarginLayoutParams marginParams) {
                marginParams.bottomMargin =
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.vertical_tab_pinned_item_margin_bottom);
            }
            setLayoutParams(rootParams);
        }
        mTitleView.setVisibility(View.GONE);
        mActionButton.setVisibility(View.GONE);

        if (mAiIndicator.getLayoutParams() instanceof LayoutParams aiParams) {
            aiParams.setMarginStart(
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen.vertical_tab_pinned_item_ai_indicator_margin_start));
            mAiIndicator.setLayoutParams(aiParams);
        }

        // Pinned rows are always icon-only, so the favicon stays centered regardless of which
        // properties have been bound so far.
        applyConstraints(
                mFaviconContainer,
                LayoutParams.PARENT_ID,
                LayoutParams.UNSET,
                LayoutParams.PARENT_ID,
                /* marginStartDimenId= */ 0,
                /* marginEndDimenId= */ 0);
    }

    /** Expands the touch target of the action button via a {@link TouchDelegate}. */
    private void updateActionButtonTouchDelegate(boolean actionWanted) {
        if (!actionWanted) {
            setTouchDelegate(null);
            return;
        }

        post(
                () -> {
                    if (!mActionButton.isAttachedToWindow()
                            || mActionButton.getVisibility() != View.VISIBLE) {
                        setTouchDelegate(null);
                        return;
                    }

                    Rect rect = new Rect();
                    mActionButton.getHitRect(rect);
                    Resources res = getResources();
                    boolean isTablet = VerticalTabUtils.isTablet(getContext());
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
                    setTouchDelegate(new TouchDelegate(rect, mActionButton));
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
