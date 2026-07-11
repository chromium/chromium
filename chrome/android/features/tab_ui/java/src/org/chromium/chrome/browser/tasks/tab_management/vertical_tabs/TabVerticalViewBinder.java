// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.LinearInterpolator;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.progressindicator.CircularProgressIndicator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R.string;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListViewBinderUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Vertical Tab List item rows. */
@NullMarked
class TabVerticalViewBinder {
    private static final float ROTATION_COLLAPSED = 0f;
    private static final float ROTATION_EXPANDED = 180f;
    private static final float ACTUATION_SPINNER_ROTATION_DEGREES = 360f;
    private static final long ACTUATION_SPINNER_DURATION_MS = 2000L;
    @VisibleForTesting static final long CHEVRON_ANIMATION_DURATION_MS = 200L;

    // Public Entry-Point Binders

    /**
     * Binds PropertyModel properties of a standard tab item to the row's ViewGroup elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the standard tab row item.
     * @param propertyKey the specific property key to bind, or null to bind all properties.
     */
    public static void bindTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {

        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            updateTitle(R.id.tab_title, model, view);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            updateRegularColors(model, view);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            updateActionButton(model, view);
        } else if (TabProperties.TAB_GROUP_ID == propertyKey) {
            updateChildRowPadding(model, view);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TabListViewBinderUtils.updateContentDescription(model, view);
        } else if (TabProperties.ACCESSIBILITY_DELEGATE == propertyKey) {
            view.setAccessibilityDelegate(model.get(TabProperties.ACCESSIBILITY_DELEGATE));
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            @Nullable View actionButton = view.findViewById(R.id.action_button);
            if (actionButton != null) {
                TabListViewBinderUtils.updateActionButtonContentDescription(model, actionButton);
            }
        } else if (TabProperties.MEDIA_INDICATOR == propertyKey) {
            updateMediaIndicator(model, view);
        } else if (TabProperties.ACTOR_UI_STATE == propertyKey) {
            updateActorIndicator(model, view);
        } else if (TabProperties.IS_RAIL_COLLAPSED == propertyKey) {
            updateTabItemSize(
                    model,
                    view,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            updateTitle(R.id.tab_title, model, view);
            updateActionButton(model, view);
            updateMediaIndicator(model, view);
            updateActorIndicator(model, view);
            updateChildRowPadding(model, view);
        }
    }

    /**
     * Binds PropertyModel properties of a compact, icon-only pinned tab row to the view elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the pinned tab row item.
     * @param propertyKey the specific property key to bind, or null to bind all properties.
     */
    public static void bindPinnedTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (view.getId() == R.id.hidden_pinned_tab) {
            return;
        }
        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            view.setContentDescription(model.get(TabProperties.TITLE));
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            updatePinnedColors(model, view);
        } else if (TabProperties.IS_RAIL_COLLAPSED == propertyKey) {
            Resources resources = view.getContext().getResources();
            updateTabItemSize(
                    model,
                    view,
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_width),
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_height));
            updateChildRowPadding(model, view);
        }
    }

    /**
     * Binds properties of a tab group header row item to its ViewGroup elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab group header row item.
     * @param propertyKey the specific property key to bind.
     */
    public static void bindTabGroupHeader(
            PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            updateTitle(R.id.group_title, model, view);
        } else if (TabProperties.TAB_GROUP_CARD_COLOR == propertyKey) {
            updateGroupHeaderColors(model, view);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TabListViewBinderUtils.updateContentDescription(model, view);
            updateAccessibilityDelegate(model, view);
        } else if (TabProperties.IS_COLLAPSED == propertyKey) {
            updateChevronRotation(model, view);
            TabListViewBinderUtils.updateContentDescription(model, view);
            updateAccessibilityDelegate(model, view);
        } else if (TabProperties.IS_RAIL_COLLAPSED == propertyKey) {
            updateTabItemSize(
                    model,
                    view,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);

            updateTitle(R.id.group_title, model, view);
            updateChildRowPadding(model, view);
        }
    }

    // Common Property Binding Helpers

    /**
     * Binds common property keys shared by all tab row views, preventing duplicate routing logic.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab row item.
     * @param propertyKey the specific property key to bind.
     */
    private static void bindCommonProperties(
            PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (TabProperties.FAVICON_FETCHER == propertyKey) {
            updateFavicon(model, view);
        } else if (TabProperties.IS_LOADING == propertyKey) {
            updateLoadingState(model, view);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableClickListener(
                    model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_CONTEXT_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableContextClickListener(
                    model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER), view, model);
        }
    }

    private static void updateFavicon(PropertyModel model, ViewGroup view) {
        @Nullable ImageView faviconView = view.findViewById(R.id.tab_favicon);
        if (faviconView == null) return;

        TabListViewBinderUtils.updateFaviconImage(model, faviconView);
        adjustFaviconVisibility(model, faviconView);
    }

    private static void updateLoadingState(PropertyModel model, ViewGroup view) {
        boolean isLoading = model.get(TabProperties.IS_LOADING);
        @Nullable CircularProgressIndicator spinner = view.findViewById(R.id.tab_loading_spinner);

        if (spinner != null) {
            if (isLoading) {
                spinner.setIndicatorColor(
                        SemanticColorUtils.getDefaultIconColorAccent1(view.getContext()));
                spinner.show();
            } else {
                spinner.setVisibility(View.GONE);
            }
        }

        @Nullable ImageView faviconView = view.findViewById(R.id.tab_favicon);
        if (faviconView != null) {
            adjustFaviconVisibility(model, faviconView);
        }
    }

    private static void adjustFaviconVisibility(PropertyModel model, ImageView faviconView) {
        if (model.get(TabProperties.FAVICON_FETCHER) == null) {
            faviconView.setVisibility(View.GONE);
            return;
        }
        boolean isLoading = model.get(TabProperties.IS_LOADING);
        faviconView.setVisibility(isLoading ? View.INVISIBLE : View.VISIBLE);
    }

    private static void updateMediaIndicator(PropertyModel model, ViewGroup view) {
        ImageView mediaIndicator = view.findViewById(R.id.media_indicator_icon);
        if (mediaIndicator == null) return;

        // TODO(b/527641177): Show media indicator instead of favicon when rail is collapsed.
        if (model.get(TabProperties.IS_RAIL_COLLAPSED)) {
            mediaIndicator.setVisibility(View.GONE);
            return;
        }

        @MediaState int mediaState = model.get(TabProperties.MEDIA_INDICATOR);
        if (mediaState != MediaState.NONE) {
            mediaIndicator.setImageResource(TabUtils.getMediaIndicatorDrawable(mediaState));
            mediaIndicator.setVisibility(View.VISIBLE);
        } else {
            mediaIndicator.setVisibility(View.GONE);
        }
    }

    private static void updateActorIndicator(PropertyModel model, ViewGroup view) {
        @Nullable View aiIndicatorLine = view.findViewById(R.id.ai_indicator);
        @Nullable ImageView actuationSpark = view.findViewById(R.id.actuation_spark);
        @Nullable ImageView actuationSpinner = view.findViewById(R.id.actuation_spinner);

        if (aiIndicatorLine == null || actuationSpark == null || actuationSpinner == null) return;

        // TODO(b/527641177): Confirm this use case when rail is collapsed.
        if (model.get(TabProperties.IS_RAIL_COLLAPSED)) {
            aiIndicatorLine.setVisibility(View.GONE);
            actuationSpark.setVisibility(View.GONE);
            actuationSpinner.setVisibility(View.GONE);
            ObjectAnimator animator =
                    (ObjectAnimator) actuationSpinner.getTag(R.id.actuation_spinner);
            if (animator != null && animator.isRunning()) {
                animator.cancel();
            }
            return;
        }

        boolean shouldBeVisible = TabListViewBinderUtils.setupActorIndicator(model, view);
        aiIndicatorLine.setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);

        @Nullable UiTabState state = model.get(TabProperties.ACTOR_UI_STATE);

        boolean isDynamic =
                shouldBeVisible
                        && state != null
                        && state.tabIndicator == TabIndicatorStatus.DYNAMIC;

        ObjectAnimator animator = (ObjectAnimator) actuationSpinner.getTag(R.id.actuation_spinner);

        if (isDynamic) {
            actuationSpark.setVisibility(View.VISIBLE);
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
            actuationSpark.setVisibility(View.GONE);
            actuationSpinner.setVisibility(View.GONE);
        }
    }

    // Row-Specific Layout Color Binder Helpers

    private static void updateRegularColors(PropertyModel model, ViewGroup view) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        Context context = view.getContext();
        view.setSelected(isSelected);

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
            ViewCompat.setBackgroundTintList(view, getBackgroundTintList(context, isSelected));
        }

        TextView titleView = view.findViewById(R.id.tab_title);
        titleView.setTextColor(getTextColor(context, isSelected));

        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);
        if (actionButton != null) {
            ImageViewCompat.setImageTintList(
                    actionButton, getActionButtonTintList(context, isSelected));
        }
        updateActionButton(model, view);

        updateFavicon(model, view);
        setupTabHoverListener(
                model,
                view,
                /* defaultBackgroundColor= */ ColorStateList.valueOf(Color.TRANSPARENT));
    }

    /**
     * Updates the background tint and website favicon specifically for a pinned tab row view.
     * Clears background tints when unselected, to allow the solid XML container drawable to render.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the pinned tab row item.
     */
    private static void updatePinnedColors(PropertyModel model, ViewGroup view) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        Context context = view.getContext();
        view.setSelected(isSelected);

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
            ColorStateList tintList =
                    isSelected ? getBackgroundTintList(context, /* isSelected= */ true) : null;
            ViewCompat.setBackgroundTintList(view, tintList);
        }
        updateFavicon(model, view);
        setupTabHoverListener(model, view, /* defaultBackgroundColor= */ null);
    }

    /**
     * Updates the background tint color specifically for the tab group header row view, dynamically
     * resolving the group color ID using TabGroupColorPickerUtils.
     *
     * @param model the model containing the tab group properties.
     * @param view the root ViewGroup representing the tab group header row item.
     */
    private static void updateGroupHeaderColors(PropertyModel model, ViewGroup view) {
        @Nullable Integer colorId = model.get(TabProperties.TAB_GROUP_CARD_COLOR);
        Context context = view.getContext();

        @Nullable Drawable bg = view.getBackground();
        if (bg != null && colorId != null) {
            bg.mutate();
            int backgroundColor =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                            context, colorId, /* isIncognito= */ false);
            ViewCompat.setBackgroundTintList(view, ColorStateList.valueOf(backgroundColor));

            @ColorInt
            int foregroundColor =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                            context, colorId, /* isIncognito= */ false);

            TextView titleView = view.findViewById(R.id.group_title);
            if (titleView != null) {
                titleView.setTextColor(foregroundColor);
            }

            @Nullable ImageView expandChevron = view.findViewById(R.id.expand_chevron);
            if (expandChevron != null) {
                ImageViewCompat.setImageTintList(
                        expandChevron, ColorStateList.valueOf(foregroundColor));
            }
        }
    }

    private static void updateTabItemSize(
            PropertyModel model, ViewGroup view, int expandedWidth, int expandedHeight) {
        boolean isRailCollapsed = model.get(TabProperties.IS_RAIL_COLLAPSED);
        Context context = view.getContext();
        ViewGroup.LayoutParams params = view.getLayoutParams();
        if (params == null) return;

        int collapsedSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_collapsed_size);
        int width = isRailCollapsed ? collapsedSize : expandedWidth;
        int height = isRailCollapsed ? collapsedSize : expandedHeight;

        if (params.width != width || params.height != height) {
            params.width = width;
            params.height = height;
            view.setLayoutParams(params);
        }
    }

    private static void updateTitle(int titleViewId, PropertyModel model, ViewGroup view) {
        TextView titleView = view.findViewById(titleViewId);
        if (titleView == null) return;

        boolean isRailCollapsed = model.get(TabProperties.IS_RAIL_COLLAPSED);
        if (isRailCollapsed) {
            titleView.setVisibility(View.GONE);
        } else {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(model.get(TabProperties.TITLE));
        }
        TabListViewBinderUtils.updateContentDescription(model, view);
    }

    private static void updateActionButton(PropertyModel model, ViewGroup view) {
        View actionButton = view.findViewById(R.id.action_button);
        if (actionButton == null) return;

        boolean isRailCollapsed = model.get(TabProperties.IS_RAIL_COLLAPSED);
        TabActionButtonData data = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
        boolean isSelected = model.get(TabProperties.IS_SELECTED);

        if (isRailCollapsed || data == null) {
            actionButton.setVisibility(View.GONE);
        } else {
            actionButton.setVisibility(isSelected ? View.VISIBLE : View.INVISIBLE);
            TabListViewBinderUtils.bindActionButton(model, actionButton, data);
        }
    }

    // Row-Specific Layout Geometry & Rotation Helpers

    private static void updateChevronRotation(PropertyModel model, ViewGroup view) {
        boolean isCollapsed = model.get(TabProperties.IS_COLLAPSED);
        @Nullable ImageView expandChevron = view.findViewById(R.id.expand_chevron);
        if (expandChevron != null) {
            expandChevron.animate().cancel();
            float targetRotation = isCollapsed ? ROTATION_COLLAPSED : ROTATION_EXPANDED;

            if (expandChevron.getRotation() == targetRotation) return;

            if (expandChevron.isAttachedToWindow()) {
                expandChevron
                        .animate()
                        .rotation(targetRotation)
                        .setDuration(CHEVRON_ANIMATION_DURATION_MS)
                        .start();
            } else {
                expandChevron.setRotation(targetRotation);
            }
        }
    }

    private static void updateAccessibilityDelegate(PropertyModel model, View view) {
        view.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        boolean isCollapsed = model.get(TabProperties.IS_COLLAPSED);
                        String actionLabel =
                                host.getContext()
                                        .getString(
                                                isCollapsed
                                                        ? string.accessibility_expand_section
                                                        : string.accessibility_collapse_section);
                        info.addAction(
                                new AccessibilityNodeInfo.AccessibilityAction(
                                        AccessibilityNodeInfo.ACTION_CLICK, actionLabel));
                    }
                });
    }

    private static void updateChildRowPadding(PropertyModel model, View view) {
        boolean isInGroup = model.get(TabProperties.TAB_GROUP_ID) != null;
        boolean isRailCollapsed = model.get(TabProperties.IS_RAIL_COLLAPSED);

        int marginStart = 0;
        Resources resources = view.getResources();
        if (isRailCollapsed) {
            marginStart =
                    resources.getDimensionPixelSize(
                            R.dimen.vertical_tab_child_collapsed_margin_start);
        } else if (isInGroup) {
            marginStart =
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
        }

        if (view.getLayoutParams() instanceof ViewGroup.MarginLayoutParams params) {
            if (params.getMarginStart() != marginStart) {
                params.setMarginStart(marginStart);
                view.setLayoutParams(params);
            }
        }
    }

    // Theme & Color Utility Methods

    private static ColorStateList getBackgroundTintList(Context context, boolean isSelected) {
        return isSelected
                ? ColorStateList.valueOf(SemanticColorUtils.getColorSurface(context))
                : ColorStateList.valueOf(Color.TRANSPARENT);
    }

    private static @ColorInt int getTextColor(Context context, boolean isSelected) {
        return isSelected
                ? SemanticColorUtils.getColorOnSurface(context)
                : SemanticColorUtils.getDefaultTextColorSecondary(context);
    }

    private static ColorStateList getActionButtonTintList(Context context, boolean isSelected) {
        return ColorStateList.valueOf(
                isSelected
                        ? SemanticColorUtils.getDefaultIconColor(context)
                        : SemanticColorUtils.getDefaultIconColorSecondary(context));
    }

    // Gesture & Interaction Layout Helpers

    private static void setupTabHoverListener(
            PropertyModel model, ViewGroup view, @Nullable ColorStateList defaultBackgroundColor) {
        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);

        view.setOnHoverListener(
                (rowView, motionEvent) -> {
                    boolean isSelected = model.get(TabProperties.IS_SELECTED);
                    // TODO(b/527641177): Show close button when hovering on the selected tab for
                    // collapsed rail.
                    boolean isRailCollapsed = model.get(TabProperties.IS_RAIL_COLLAPSED);
                    if (isSelected) {
                        if (actionButton != null) {
                            actionButton.setVisibility(isRailCollapsed ? View.GONE : View.VISIBLE);
                        }
                        return false;
                    }

                    switch (motionEvent.getAction()) {
                        case MotionEvent.ACTION_HOVER_ENTER:
                            if (actionButton != null && !isRailCollapsed) {
                                actionButton.setVisibility(View.VISIBLE);
                            }
                            ViewCompat.setBackgroundTintList(
                                    view,
                                    ColorStateList.valueOf(
                                            TabUiThemeUtil.getHoveredTabContainerColor(
                                                    view.getContext(), /* isIncognito= */ false)));
                            break;
                        case MotionEvent.ACTION_HOVER_EXIT:
                            if (actionButton != null
                                    && (actionButton.isHovered()
                                            || isPointWithinChild(
                                                    actionButton,
                                                    motionEvent.getX(),
                                                    motionEvent.getY()))) {
                                break;
                            }
                            if (actionButton != null) {
                                actionButton.setVisibility(View.INVISIBLE);
                            }
                            ViewCompat.setBackgroundTintList(view, defaultBackgroundColor);
                            break;
                    }
                    return false;
                });

        if (actionButton != null) {
            actionButton.setOnHoverListener(
                    (v, motionEvent) -> {
                        if (motionEvent.getAction() == MotionEvent.ACTION_HOVER_EXIT) {
                            float xInView = v.getLeft() + motionEvent.getX();
                            float yInView = v.getTop() + motionEvent.getY();
                            if (xInView < 0
                                    || xInView >= view.getWidth()
                                    || yInView < 0
                                    || yInView >= view.getHeight()) {
                                if (!model.get(TabProperties.IS_SELECTED)) {
                                    actionButton.setVisibility(View.INVISIBLE);
                                    ViewCompat.setBackgroundTintList(view, defaultBackgroundColor);
                                }
                            }
                        }
                        return false;
                    });
        }
    }

    private static boolean isPointWithinChild(
            @Nullable View child, float xInParent, float yInParent) {
        return child != null
                && child.getVisibility() == View.VISIBLE
                && xInParent >= child.getLeft()
                && xInParent < child.getRight()
                && yInParent >= child.getTop()
                && yInParent < child.getBottom();
    }
}
