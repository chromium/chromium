// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID;
import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.LinearInterpolator;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.progressindicator.CircularProgressIndicator;

import org.chromium.base.DeviceInfo;
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
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardHelper.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.DeviceFormFactor;
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
            updateIcons(model, view);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            View actionButton = view.findViewById(R.id.action_button);
            if (actionButton != null) {
                TabListViewBinderUtils.bindActionButton(
                        model, actionButton, model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
            }
            updateIcons(model, view);
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
            ImageView mediaIndicator = view.findViewById(R.id.media_indicator_icon);
            if (mediaIndicator != null) {
                @MediaState int mediaState = model.get(TabProperties.MEDIA_INDICATOR);
                if (mediaState != MediaState.NONE) {
                    mediaIndicator.setImageResource(TabUtils.getMediaIndicatorDrawable(mediaState));
                }
            }
            updateIcons(model, view);
        } else if (TabProperties.ACTOR_UI_STATE == propertyKey) {
            TabListViewBinderUtils.setupActorIndicator(model, view);
            updateIcons(model, view);
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
            updateTabItemSize(
                    model,
                    view,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            updateTitle(R.id.tab_title, model, view);
            updateChildRowPadding(model, view);
            updateParentPadding(model, view);
            updateIcons(model, view);
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
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
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
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
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
            updateFaviconImage(model, view);
        } else if (TabProperties.IS_LOADING == propertyKey) {
            updateIcons(model, view);
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

    // Icon Update Helpers.
    // Icons priority when rail is collapsed: action > ai > media > loading > favicon

    private static void updateFaviconImage(PropertyModel model, ViewGroup view) {
        @Nullable ImageView faviconView = view.findViewById(R.id.tab_favicon);
        if (faviconView == null) return;

        TabListViewBinderUtils.updateFaviconImage(model, faviconView);
        updateIcons(model, view);
    }

    private static void updateIcons(PropertyModel model, ViewGroup view) {
        updateIcons(model, view, view.isHovered());
    }

    // TODO(crbug.com/527641177): Add icons for pinned tab and check priorities.
    private static void updateIcons(PropertyModel model, ViewGroup view, boolean isHovered) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        boolean isSelected = model.get(TabProperties.IS_SELECTED);

        View actionButton = view.findViewById(R.id.action_button);
        View aiIndicatorLine = view.findViewById(R.id.ai_indicator);
        View actuationSpark = view.findViewById(R.id.actuation_spark);
        ImageView actuationSpinner = view.findViewById(R.id.actuation_spinner);
        ImageView mediaIndicator = view.findViewById(R.id.media_indicator_icon);
        CircularProgressIndicator spinner = view.findViewById(R.id.tab_loading_spinner);
        ImageView faviconView = view.findViewById(R.id.tab_favicon);

        // 1. Resolve independent "wanted" states
        TabActionButtonData actionData = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
        boolean actionWanted =
                actionButton != null
                        && actionData != null
                        && (isRailCollapsed
                                ? (isSelected && isHovered)
                                : (isSelected || isHovered));
        @Nullable UiTabState actorState = model.get(TabProperties.ACTOR_UI_STATE);
        boolean aiWanted =
                aiIndicatorLine != null
                        && actuationSpark != null
                        && actuationSpinner != null
                        && actorState != null
                        && actorState.tabIndicator != TabIndicatorStatus.NONE;
        @MediaState int mediaState = model.get(TabProperties.MEDIA_INDICATOR);
        boolean mediaWanted = mediaIndicator != null && mediaState != MediaState.NONE;
        boolean loadingWanted = spinner != null && model.get(TabProperties.IS_LOADING);
        boolean faviconWanted =
                faviconView != null
                        && model.get(TabProperties.FAVICON_FETCHER) != null
                        && !loadingWanted;

        // 2. Apply priority rules for collapsed state
        if (isRailCollapsed) {
            if (actionWanted) {
                aiWanted = false;
                mediaWanted = false;
                loadingWanted = false;
                faviconWanted = false;
            } else if (aiWanted) {
                mediaWanted = false;
                loadingWanted = false;
                faviconWanted = false;
            } else if (mediaWanted) {
                loadingWanted = false;
                faviconWanted = false;
            }
        }

        // 3. Apply to views, handle animations/bindings and view constraints

        // Action Button
        if (actionButton != null) {
            updateViewConstraints(
                    actionButton,
                    isRailCollapsed,
                    UNSET,
                    UNSET,
                    PARENT_ID,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ 0);
            actionButton.setVisibility(actionWanted ? View.VISIBLE : View.INVISIBLE);
            if (DeviceFormFactor.isTablet() && !DeviceInfo.isDesktop()) {
                setActionButtonTouchDelegate(view, actionButton, actionWanted);
            }
        }

        // AI Indicator
        if (aiIndicatorLine != null && actuationSpark != null && actuationSpinner != null) {
            updateAiAnimations(model, actuationSpark, actuationSpinner, aiWanted);
            updateViewConstraints(
                    actuationSpark,
                    isRailCollapsed,
                    UNSET,
                    R.id.media_indicator_icon,
                    UNSET,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ R.dimen.vertical_tab_item_media_indicator_margin_end);
            aiIndicatorLine.setVisibility(aiWanted ? View.VISIBLE : View.GONE);
        }

        // Media Indicator
        if (mediaIndicator != null) {
            updateViewConstraints(
                    mediaIndicator,
                    isRailCollapsed,
                    UNSET,
                    R.id.action_button,
                    UNSET,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ R.dimen.vertical_tab_item_media_indicator_margin_end);
            mediaIndicator.setVisibility(mediaWanted ? View.VISIBLE : View.GONE);
        }

        // Favicon container constraints (loading spinner or tab favicon)
        View faviconContainer = view.findViewById(R.id.favicon_container);
        if (faviconContainer != null && (loadingWanted || faviconWanted)) {
            updateViewConstraints(
                    faviconContainer,
                    isRailCollapsed,
                    PARENT_ID,
                    UNSET,
                    UNSET,
                    /* marginStartDimenId= */ R.dimen.vertical_tab_item_padding_horizontal,
                    /* marginEndDimenId= */ 0);
        }

        // Loading Spinner
        if (spinner != null) {
            if (loadingWanted) {
                spinner.setIndicatorColor(
                        SemanticColorUtils.getDefaultIconColorAccent1(view.getContext()));
                spinner.show();
            } else {
                spinner.setVisibility(View.GONE);
            }
        }

        // Favicon
        if (faviconView != null) {
            faviconView.setVisibility(faviconWanted ? View.VISIBLE : View.GONE);
        }
    }

    private static void setActionButtonTouchDelegate(
            ViewGroup view, View actionButton, boolean actionWanted) {
        if (!actionWanted) {
            view.setTouchDelegate(null);
            return;
        }

        view.post(
                () -> {
                    if (!actionButton.isAttachedToWindow()
                            || actionButton.getVisibility() != View.VISIBLE) {
                        view.setTouchDelegate(null);
                        return;
                    }

                    Rect rect = new Rect();
                    actionButton.getHitRect(rect);
                    Resources res = view.getResources();
                    int minTouchTargetWidthPx =
                            res.getDimensionPixelSize(R.dimen.min_touch_target_size);
                    int minTouchTargetHeightPx =
                            res.getDimensionPixelSize(
                                    R.dimen.vertical_tab_action_button_touch_target_height);

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
                    view.setTouchDelegate(new TouchDelegate(rect, actionButton));
                });
    }

    private static void updateAiAnimations(
            PropertyModel model,
            View actuationSpark,
            ImageView actuationSpinner,
            boolean aiWanted) {
        @Nullable UiTabState state = model.get(TabProperties.ACTOR_UI_STATE);
        boolean isDynamic =
                aiWanted && state != null && state.tabIndicator == TabIndicatorStatus.DYNAMIC;

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

    private static void updateViewConstraints(
            View view,
            boolean isRailCollapsed,
            int startToStart,
            int endToStart,
            int endToEnd,
            int marginStartDimenId,
            int marginEndDimenId) {
        if (isRailCollapsed) {
            configureConstraints(
                    view,
                    /* startToStart= */ PARENT_ID,
                    /* endToStart= */ UNSET,
                    /* endToEnd= */ PARENT_ID,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ 0);
        } else {
            configureConstraints(
                    view, startToStart, endToStart, endToEnd, marginStartDimenId, marginEndDimenId);
        }
    }

    private static void configureConstraints(
            View view,
            int startToStart,
            int endToStart,
            int endToEnd,
            int marginStartDimenId,
            int marginEndDimenId) {
        if (view.getLayoutParams() instanceof ConstraintLayout.LayoutParams params) {
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
            params.startToEnd = ConstraintLayout.LayoutParams.UNSET;
            params.endToStart = endToStart;
            params.endToEnd = endToEnd;
            params.setMarginStart(marginStart);
            params.setMarginEnd(marginEnd);
            view.setLayoutParams(params);
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
        updateFaviconImage(model, view);
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
        updateFaviconImage(model, view);
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
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
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

        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        if (isRailCollapsed) {
            titleView.setVisibility(View.GONE);
        } else {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(model.get(TabProperties.TITLE));
        }
        TabListViewBinderUtils.updateContentDescription(model, view);
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
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;

        int marginStart = 0;
        if (isRailCollapsed) {
            marginStart = getCollapsedChildMarginStart(view.getContext());
        } else if (isInGroup) {
            marginStart =
                    view.getResources()
                            .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
        }

        if (view.getLayoutParams() instanceof ViewGroup.MarginLayoutParams params) {
            if (params.getMarginStart() != marginStart) {
                params.setMarginStart(marginStart);
                view.setLayoutParams(params);
            }
        }
    }

    /**
     * Calculates the start margin in pixels for a tab item when the rail is collapsed.
     *
     * <p>Horizontally centers the tab item within the collapsed rail container. Because the parent
     * RecyclerView is asymmetric due to the scrollbar end margin, the child item's start margin is
     * explicitly computed as: (rail_collapsed_width - tab_item_collapsed_size) / 2 -
     * rail_horizontal_margin.
     */
    @VisibleForTesting
    static int getCollapsedChildMarginStart(Context context) {
        Resources resources = context.getResources();
        int railWidth =
                ViewUtils.dpToPx(context, VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        int itemSize = resources.getDimensionPixelSize(R.dimen.vertical_tab_item_collapsed_size);
        int railStartMargin =
                resources.getDimensionPixelSize(R.dimen.vertical_tabs_rail_horizontal_margin);
        return (railWidth - itemSize) / 2 - railStartMargin;
    }

    private static void updateParentPadding(PropertyModel model, ViewGroup view) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        Context context = view.getContext();
        Resources resources = context.getResources();
        if (isRailCollapsed) {
            view.setPadding(0, 0, 0, 0);
        } else {
            int paddingHorizontal =
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_item_padding_horizontal);
            int paddingVertical =
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_item_padding_vertical);
            view.setPaddingRelative(0, paddingVertical, paddingHorizontal, paddingVertical);
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
                (v, motionEvent) -> {
                    boolean isSelected = model.get(TabProperties.IS_SELECTED);
                    switch (motionEvent.getAction()) {
                        case MotionEvent.ACTION_HOVER_ENTER:
                            // TODO(crbug.com/533531896): Handle clearing all backgrounds before
                            // showing tab background.
                            // TODO(crbug.com/527641177): Maybe show a darker background color for
                            // action button when it's being hovered?
                            if (!isSelected) {
                                ViewCompat.setBackgroundTintList(
                                        view,
                                        ColorStateList.valueOf(
                                                TabUiThemeUtil.getHoveredTabContainerColor(
                                                        view.getContext(),
                                                        /* isIncognito= */ false)));
                            }
                            updateIcons(model, view, /* isHovered= */ true);
                            notifyHoverChange(model, view, /* isHovered= */ true);
                            return true;
                        case MotionEvent.ACTION_HOVER_EXIT:
                            float x = motionEvent.getX();
                            float y = motionEvent.getY();
                            if (x < 0 || x >= view.getWidth() || y < 0 || y >= view.getHeight()) {
                                if (!isSelected) {
                                    ViewCompat.setBackgroundTintList(view, defaultBackgroundColor);
                                }
                                updateIcons(model, view, /* isHovered= */ false);
                                notifyHoverChange(model, view, /* isHovered= */ false);
                            }
                            return true;
                    }
                    return false;
                });

        if (actionButton != null) {
            actionButton.setOnHoverListener(
                    (v, motionEvent) -> {
                        int action = motionEvent.getAction();
                        if (action == MotionEvent.ACTION_HOVER_ENTER) {
                            v.setHovered(true);
                            if (!model.get(TabProperties.IS_SELECTED)) {
                                ViewCompat.setBackgroundTintList(
                                        view,
                                        ColorStateList.valueOf(
                                                TabUiThemeUtil.getHoveredTabContainerColor(
                                                        view.getContext(),
                                                        /* isIncognito= */ false)));
                            }
                            updateIcons(model, view, /* isHovered= */ true);
                            notifyHoverChange(model, view, /* isHovered= */ true);
                            return true;
                        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
                            v.setHovered(false);
                            float xInView = v.getLeft() + motionEvent.getX();
                            float yInView = v.getTop() + motionEvent.getY();
                            if (xInView < 0
                                    || xInView >= view.getWidth()
                                    || yInView < 0
                                    || yInView >= view.getHeight()) {
                                if (!model.get(TabProperties.IS_SELECTED)) {
                                    ViewCompat.setBackgroundTintList(view, defaultBackgroundColor);
                                }
                                updateIcons(model, view, /* isHovered= */ false);
                                notifyHoverChange(model, view, /* isHovered= */ false);
                            }
                            return true;
                        }
                        return false;
                    });
        }
    }

    private static void notifyHoverChange(PropertyModel model, View view, boolean isHovered) {
        TabHoverCardListener listener = model.get(TabProperties.TAB_HOVER_CARD_LISTENER);
        if (listener != null) {
            int tabId = model.get(TabProperties.TAB_ID);
            listener.onTabHoverCardStateChanged(tabId, view, isHovered);
        }
    }
}
