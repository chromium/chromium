// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID;
import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET;

import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.progressindicator.CircularProgressIndicator;

import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListViewBinderUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
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

    private static boolean isTablet(Context context) {
        return VerticalTabUtils.isTablet(context);
    }

    // Public Entry-Point Binders

    /**
     * Binds PropertyModel properties of a standard tab item to the row's ViewGroup elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the standard tab row item.
     * @param propertyKey the specific property key to bind, or null to bind all properties.
     */
    static void bindTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {

        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            updateTitle(R.id.tab_title, model, view);
            updateParentPadding(model, view, /* isHeader= */ false);
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.IS_MULTI_SELECTED == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updateRegularColors(model, view);
            updateIcons(model, view);
            updateParentPadding(model, view, /* isHeader= */ false);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            View actionButton = view.findViewById(R.id.action_button);
            if (actionButton != null) {
                TabListViewBinderUtils.bindActionButton(
                        model, actionButton, model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
            }
            updateIcons(model, view);
        } else if (TabProperties.TAB_GROUP_ID == propertyKey) {
            updateChildRowPadding(model, view);
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
            int itemHeight = getTabItemHeight(view.getContext());
            updateTabItemSize(model, view, ViewGroup.LayoutParams.MATCH_PARENT, itemHeight);
            updateTitle(R.id.tab_title, model, view);
            updateChildRowPadding(model, view);
            updateParentPadding(model, view, /* isHeader= */ false);
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
    static void bindPinnedTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (view.getId() == R.id.hidden_pinned_tab) {
            return;
        }
        bindCommonProperties(model, view, propertyKey);

        int pinnedHeight = getPinnedItemHeight(view.getContext());
        int expandedWidth = ViewGroup.LayoutParams.MATCH_PARENT;
        updateTabItemSize(model, view, expandedWidth, pinnedHeight);

        if (TabProperties.TITLE == propertyKey || TabProperties.IS_PINNED == propertyKey) {
            updateContentDescription(model, view);
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.IS_MULTI_SELECTED == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updatePinnedColors(model, view);
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
            updateTabItemSize(model, view, expandedWidth, pinnedHeight);
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
    static void bindTabGroupHeader(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            updateTitle(R.id.group_title, model, view);
            updateParentPadding(model, view, /* isHeader= */ true);
        } else if (TabProperties.TAB_GROUP_CARD_COLOR == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updateGroupHeaderColors(model, view);
        } else if (TabProperties.IS_COLLAPSED == propertyKey) {
            updateChevronRotation(model, view);
            updateContentDescription(model, view);
        } else if (TabProperties.RAIL_COLLAPSE_STATE == propertyKey) {
            int itemHeight = getTabItemHeight(view.getContext());
            updateTabItemSize(model, view, ViewGroup.LayoutParams.MATCH_PARENT, itemHeight);
            updateTitle(R.id.group_title, model, view);
            updateChildRowPadding(model, view);
            updateParentPadding(model, view, /* isHeader= */ true);
            updateGroupHeaderIcons(model, view, view.isHovered());
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            View menuButton = view.findViewById(R.id.menu_button);
            if (menuButton != null) {
                TabListViewBinderUtils.bindActionButton(
                        model, menuButton, model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
            }
            setupTabGroupHeaderHoverListener(model, view);
        }
    }

    static int getTabItemHeight(Context context) {
        return context.getResources()
                .getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_item_height_tablet
                                : R.dimen.vertical_tab_item_height);
    }

    static int getPinnedItemHeight(Context context) {
        return context.getResources()
                .getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_pinned_item_height_tablet
                                : R.dimen.vertical_tab_pinned_item_height);
    }

    static int getPinnedItemMinWidth(Context context) {
        return context.getResources()
                .getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_pinned_item_min_width_tablet
                                : R.dimen.vertical_tab_pinned_item_min_width);
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
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        if (view.getVisibility() != View.VISIBLE) {
            view.setVisibility(View.VISIBLE);
        }
        if (view.getAlpha() != 1.0f) {
            view.setAlpha(1.0f);
        }
        // Since TAB_CONTEXT_CLICK_LISTENER is now disabled for Vertical Tabs, consume context
        // clicks at the child tab item level so performContextClick() does not bubble up to the
        // parent RecyclerView's context click listener. Tab item right-clicks are handled via the
        // RecyclerView's onInterceptTouchEvent.
        view.setOnContextClickListener(v -> true);

        if (TabProperties.FAVICON_FETCHER == propertyKey) {
            updateFaviconImage(model, view);
        } else if (TabProperties.IS_LOADING == propertyKey) {
            updateIcons(model, view);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            setNullableClickListener(model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_CONTEXT_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableContextClickListener(
                    model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER), view, model);
        } else if (TabProperties.MEDIA_INDICATOR == propertyKey) {
            updateMediaIndicator(model, view);
            updateIcons(model, view);
            updateContentDescription(model, view);
        } else if (TabProperties.ACTOR_UI_STATE == propertyKey) {
            updateIcons(model, view);
            updateContentDescription(model, view);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            updateContentDescription(model, view);
        } else if (TabProperties.ACCESSIBILITY_DELEGATE == propertyKey) {
            view.setAccessibilityDelegate(model.get(TabProperties.ACCESSIBILITY_DELEGATE));
        } else if (TabProperties.IS_GLIC_ACTIVE == propertyKey) {
            updateGlicIndicatorBar(model, view);
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            View button = view.findViewById(R.id.action_button);
            if (button == null) button = view.findViewById(R.id.menu_button);
            if (button != null) {
                TabListViewBinderUtils.updateActionButtonContentDescription(model, button);
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private static void setNullableClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnTouchListener(null);
            view.setOnClickListener(null);
        } else {
            // A passive tracker that records the last MotionEvent state but does not consume the
            // touch. This ensures standard Android view ripples and other touch handling remain
            // fully intact.
            final MotionEventInfo[] lastMotion = new MotionEventInfo[1];
            view.setOnTouchListener(
                    (View _, MotionEvent event) -> {
                        lastMotion[0] = MotionEventInfo.fromMotionEvent(event);
                        return false;
                    });

            view.setOnClickListener(
                    (View _) ->
                            listener.run(
                                    view, propertyModel.get(TabProperties.TAB_ID), lastMotion[0]));
        }
    }

    // Icon Update Helpers.
    // Icons priority when rail is collapsed: action > recording/sharing media > ai actuation >
    // standard media > loading > favicon

    private static void updateFaviconImage(PropertyModel model, ViewGroup view) {
        @Nullable ImageView faviconView = view.findViewById(R.id.tab_favicon);
        if (faviconView == null) return;

        TabListViewBinderUtils.updateFaviconImage(model, faviconView);
        updateIcons(model, view);
    }

    private static void updateIcons(PropertyModel model, ViewGroup view) {
        updateIcons(model, view, view.isHovered());
    }

    private static void updateIcons(PropertyModel model, ViewGroup view, boolean isHovered) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        boolean isPinned = TabProperties.isPinnedTab(model);
        boolean isIconCompact = isRailCollapsed || isPinned;
        boolean isSelected = model.get(TabProperties.IS_SELECTED);

        View actionButton = view.findViewById(R.id.action_button);
        View actuationSpark = view.findViewById(R.id.actuation_spark);
        ImageView actuationSpinner = view.findViewById(R.id.actuation_spinner);
        ImageView mediaIndicator = view.findViewById(R.id.media_indicator_icon);
        CircularProgressIndicator spinner = view.findViewById(R.id.tab_loading_spinner);
        ImageView faviconView = view.findViewById(R.id.tab_favicon);

        // 1. Resolve independent "wanted" states
        TabActionButtonData actionData = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
        // Close button is always visible on touch devices, but only visible on select/hover on
        // desktop.
        boolean actionWanted =
                actionButton != null
                        && actionData != null
                        && (isIconCompact
                                ? (isSelected && (!DeviceInfo.isDesktop() || isHovered))
                                : (!DeviceInfo.isDesktop() || isSelected || isHovered));
        @Nullable UiTabState actorState = model.get(TabProperties.ACTOR_UI_STATE);
        boolean actorActuationWanted =
                actuationSpark != null
                        && actuationSpinner != null
                        && TabListViewBinderUtils.isActorActive(actorState);
        @MediaState int mediaState = model.get(TabProperties.MEDIA_INDICATOR);
        boolean mediaWanted = mediaIndicator != null && mediaState != MediaState.NONE;
        boolean loadingWanted = spinner != null && model.get(TabProperties.IS_LOADING);
        boolean faviconWanted =
                faviconView != null
                        && model.get(TabProperties.FAVICON_FETCHER) != null
                        && !loadingWanted;

        // 2. Apply priority rules for collapsed state.
        // Priority: Close > Recording/Sharing Media > AI Actuation > Standard Media >
        // Loading/Favicon.
        if (isIconCompact) {
            boolean isRecordingOrSharing =
                    mediaState == MediaState.RECORDING || mediaState == MediaState.SHARING;
            boolean recordingOrSharingWanted = mediaWanted && isRecordingOrSharing;
            boolean standardMediaWanted = mediaWanted && !isRecordingOrSharing;

            if (actionWanted) {
                actorActuationWanted = false;
                mediaWanted = false;
                loadingWanted = false;
                faviconWanted = false;
            } else if (recordingOrSharingWanted) {
                actorActuationWanted = false;
                loadingWanted = false;
                faviconWanted = false;
            } else if (actorActuationWanted) {
                mediaWanted = false;
                loadingWanted = false;
                faviconWanted = false;
            } else if (standardMediaWanted) {
                loadingWanted = false;
                faviconWanted = false;
            }
        }

        // 3. Apply to views, handle animations/bindings and view constraints

        // Action Button
        if (actionButton != null) {
            updateViewConstraints(
                    actionButton,
                    isIconCompact,
                    UNSET,
                    UNSET,
                    PARENT_ID,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ 0);
            actionButton.setVisibility(actionWanted ? View.VISIBLE : View.GONE);
            setActionButtonTouchDelegate(view, actionButton, actionWanted);
        }

        // Actor Actuation Indicator Icons
        if (actuationSpark != null && actuationSpinner != null) {
            updateActorAnimations(model, actuationSpark, actuationSpinner, actorActuationWanted);
            updateViewConstraints(
                    actuationSpark,
                    isIconCompact,
                    UNSET,
                    R.id.media_indicator_icon,
                    UNSET,
                    /* marginStartDimenId= */ 0,
                    /* marginEndDimenId= */ R.dimen.vertical_tab_item_media_indicator_margin_end);
        }

        // Media Indicator
        if (mediaIndicator != null) {
            updateViewConstraints(
                    mediaIndicator,
                    isIconCompact,
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
                    isIconCompact,
                    PARENT_ID,
                    UNSET,
                    UNSET,
                    /* marginStartDimenId= */ R.dimen.vertical_tab_item_padding_horizontal,
                    /* marginEndDimenId= */ 0);
        }

        // Loading Spinner
        if (spinner != null) {
            if (loadingWanted) {
                boolean isIncognito = isIncognito(model);
                int spinnerColor =
                        isIncognito
                                ? view.getContext().getColor(R.color.default_icon_color_blue_light)
                                : SemanticColorUtils.getDefaultIconColorAccent1(view.getContext());
                spinner.setIndicatorColor(spinnerColor);
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
                    boolean isTablet = isTablet(view.getContext());
                    int minTouchTargetWidthPx =
                            res.getDimensionPixelSize(
                                    isTablet
                                            ? R.dimen
                                                    .vertical_tab_action_button_touch_target_width_tablet
                                            : R.dimen.vertical_tab_action_button_touch_target_size);
                    int minTouchTargetHeightPx =
                            res.getDimensionPixelSize(
                                    isTablet
                                            ? R.dimen
                                                    .vertical_tab_action_button_touch_target_height_tablet
                                            : R.dimen.vertical_tab_action_button_touch_target_size);

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

    /** Updates the visibility of the Glic indicator bar on the tab row. */
    private static void updateGlicIndicatorBar(PropertyModel model, View view) {
        View glicIndicatorView = view.findViewById(R.id.ai_indicator);
        if (glicIndicatorView == null) return;

        boolean isGlicActive =
                model.containsKey(TabProperties.IS_GLIC_ACTIVE)
                        && model.get(TabProperties.IS_GLIC_ACTIVE);
        glicIndicatorView.setVisibility(isGlicActive ? View.VISIBLE : View.GONE);
    }

    /**
     * Updates the media indicator icon drawable based on the current media state of the tab.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab row item.
     */
    private static void updateMediaIndicator(PropertyModel model, ViewGroup view) {
        ImageView mediaIndicator = view.findViewById(R.id.media_indicator_icon);
        if (mediaIndicator != null) {
            @MediaState int mediaState = model.get(TabProperties.MEDIA_INDICATOR);
            if (mediaState != MediaState.NONE) {
                mediaIndicator.setImageResource(TabUtils.getMediaIndicatorDrawable(mediaState));
            }
            boolean isIncognito = isIncognito(model);
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            Context context = view.getContext();

            @ColorInt
            int defaultIconColor =
                    getActionButtonTintList(context, isSelected, isIncognito).getDefaultColor();

            ImageViewCompat.setImageTintList(
                    mediaIndicator,
                    ColorStateList.valueOf(
                            TabUtils.getMediaIndicatorTintColor(
                                    context, mediaState, defaultIconColor)));
        }
    }

    private static void updateActorAnimations(
            PropertyModel model,
            View actuationSpark,
            ImageView actuationSpinner,
            boolean actorActuationWanted) {
        @Nullable UiTabState state = model.get(TabProperties.ACTOR_UI_STATE);
        boolean isDynamic =
                actorActuationWanted
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

    private static void updateViewConstraints(
            View view,
            boolean isIconCompact,
            int startToStart,
            int endToStart,
            int endToEnd,
            int marginStartDimenId,
            int marginEndDimenId) {
        if (isIconCompact) {
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

    /**
     * Updates the selected visual/accessibility state, background color tints, website favicon, and
     * media indicator for both standard and pinned vertical tab rows.
     *
     * <p>If active tab selection or multi-selection is enabled on this tab row, resolves and
     * mutates the background drawable with the selection color matching the current incognito
     * state. Otherwise, reverts the background color to the provided default background tint.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab row item.
     * @param defaultBgColor the background tint to restore when this tab is neither active nor
     *     multi-selected.
     */
    private static void updateSelectionAndBackground(
            PropertyModel model, ViewGroup view, @Nullable ColorStateList defaultBgColor) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        boolean isMultiSelected = model.get(TabProperties.IS_MULTI_SELECTED);
        boolean isIncognito = isIncognito(model);
        Context context = view.getContext();
        view.setSelected(isSelected || isMultiSelected);

        ColorStateList tintList;
        if (isSelected || isMultiSelected) {
            tintList = getBackgroundTintList(context, isSelected, isMultiSelected, isIncognito);
        } else {
            tintList = defaultBgColor;
        }

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
            ViewCompat.setBackgroundTintList(view, tintList);
        }
        updateFaviconImage(model, view);
        updateMediaIndicator(model, view);
        setupTabHoverListener(model, view, /* defaultBackgroundColor= */ tintList);
    }

    /**
     * Updates the selection state, background tint, text colors, and action button tints for a
     * standard vertical tab row view.
     *
     * <p>When the active tab model is incognito (in a shared window), dark incognito palette colors
     * are dynamically bound for background tints, title text, and action buttons. When unselected,
     * the background remains transparent and title/action button colors use muted incognito tints.
     * When selected, a dark surface tint is applied with high-contrast text and icon colors.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the standard tab row item.
     */
    private static void updateRegularColors(PropertyModel model, ViewGroup view) {
        updateSelectionAndBackground(model, view, ColorStateList.valueOf(Color.TRANSPARENT));
        updateBackgroundInsets(view);

        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        boolean isIncognito = isIncognito(model);
        Context context = view.getContext();

        TextView titleView = view.findViewById(R.id.tab_title);
        titleView.setTextColor(getTextColor(context, isSelected, isIncognito));

        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);
        if (actionButton != null) {
            ImageViewCompat.setImageTintList(
                    actionButton, getActionButtonTintList(context, isSelected, isIncognito));
        }
    }

    /**
     * Updates the background tint and website favicon specifically for a pinned tab row view.
     *
     * <p>In regular mode, unselected pinned tabs clear background tints (set to {@code null}) to
     * allow the solid XML container drawable to render. In incognito mode on foldables (shared
     * window), unselected pinned tabs use the dark baseline surface container high tint ({@link
     * R.color#gm3_baseline_surface_container_high_dark}) to provide a distinct pill container
     * without dynamic colors, and selected pinned tabs use the dark surface background tint.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the pinned tab row item.
     */
    private static void updatePinnedColors(PropertyModel model, ViewGroup view) {
        boolean isIncognito = isIncognito(model);
        @Nullable ColorStateList defaultBackgroundColor =
                isIncognito
                        ? ColorStateList.valueOf(
                                view.getContext()
                                        .getColor(R.color.gm3_baseline_surface_container_high_dark))
                        : null;
        updateSelectionAndBackground(model, view, defaultBackgroundColor);
    }

    /**
     * Updates the background tint, title text color, and chevron icon tint specifically for the tab
     * group header row view.
     *
     * <p>Dynamically resolves group color tints and foreground text/icon colors using {@link
     * TabGroupColorPickerUtils} with the {@code isIncognito} parameter. If no explicit group color
     * ID is set on an incognito group header, fallback card background and title colors from {@link
     * TabCardThemeUtil} are applied so the header matches the dark incognito styling.
     *
     * @param model the model containing the tab group properties.
     * @param view the root ViewGroup representing the tab group header row item.
     */
    private static void updateGroupHeaderColors(PropertyModel model, ViewGroup view) {
        @Nullable Integer colorId = model.get(TabProperties.TAB_GROUP_CARD_COLOR);
        boolean isIncognito = isIncognito(model);
        Context context = view.getContext();
        updateBackgroundInsets(view);

        @Nullable Drawable bg = view.getBackground();
        if (bg == null || (colorId == null && !isIncognito)) {
            return;
        }
        bg.mutate();
        int backgroundColor =
                colorId != null
                        ? TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                context, colorId, isIncognito)
                        : TabCardThemeUtil.getCardViewBackgroundColor(
                                context, isIncognito, /* isSelected= */ false, /* colorId= */ null);
        ViewCompat.setBackgroundTintList(view, ColorStateList.valueOf(backgroundColor));
        view.invalidate();

        @ColorInt
        int foregroundColor =
                colorId != null
                        ? TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                                context, colorId, isIncognito)
                        : TabCardThemeUtil.getTitleTextColor(
                                context, isIncognito, /* isSelected= */ false, /* colorId= */ null);

        TextView titleView = view.findViewById(R.id.group_title);
        if (titleView != null) {
            titleView.setTextColor(foregroundColor);
        }

        @Nullable ImageView expandChevron = view.findViewById(R.id.expand_chevron);
        if (expandChevron != null) {
            ImageViewCompat.setImageTintList(
                    expandChevron, ColorStateList.valueOf(foregroundColor));
        }

        @Nullable ImageView menuButton = view.findViewById(R.id.menu_button);
        if (menuButton != null) {
            ImageViewCompat.setImageTintList(menuButton, ColorStateList.valueOf(foregroundColor));
        }

        view.setForegroundTintList(ColorStateList.valueOf(foregroundColor));
    }

    private static void updateTabItemSize(
            PropertyModel model, ViewGroup view, int expandedWidth, int expandedHeight) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        Context context = view.getContext();
        ViewGroup.LayoutParams params = view.getLayoutParams();
        if (params == null) return;

        int width = isRailCollapsed ? getCollapsedTabItemWidth(context) : expandedWidth;
        int height = isRailCollapsed ? getCollapsedTabItemHeight(context) : expandedHeight;

        if (params.width != width || params.height != height) {
            params.width = width;
            params.height = height;
            view.setLayoutParams(params);
        }
    }

    private static int getCollapsedTabItemWidth(Context context) {
        return context.getResources()
                .getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_item_collapsed_width_tablet
                                : R.dimen.vertical_tab_item_collapsed_size);
    }

    private static int getCollapsedTabItemHeight(Context context) {
        return context.getResources()
                .getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_item_collapsed_height_tablet
                                : R.dimen.vertical_tab_item_collapsed_size);
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
        updateContentDescription(model, view);
    }

    /**
     * Updates the content description of a tab row. Tab groups use {@link
     * TabProperties#CONTENT_DESCRIPTION_TEXT_RESOLVER}. Individual tabs format a compound string:
     * {@code "<Title> [ - Gemini is working...], [Pinned] [Media] Tab"}.
     */
    private static void updateContentDescription(PropertyModel model, View view) {
        // TODO(crbug.com/509226293): Align tab group header accessibility descriptions with HTS.
        Context context = view.getContext();
        @Nullable TextResolver contentDescriptionTextResolver =
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER);
        @Nullable CharSequence contentDescriptionString =
                contentDescriptionTextResolver != null
                        ? contentDescriptionTextResolver.resolve(context)
                        : null;
        if (TextUtils.isEmpty(contentDescriptionString) && model.containsKey(TabProperties.TITLE)) {
            String title = model.get(TabProperties.TITLE);
            boolean isPinned = TabProperties.isPinnedTab(model);
            @MediaState
            int mediaState =
                    model.containsKey(TabProperties.MEDIA_INDICATOR)
                            ? model.get(TabProperties.MEDIA_INDICATOR)
                            : MediaState.NONE;

            @Nullable UiTabState actorState =
                    model.containsKey(TabProperties.ACTOR_UI_STATE)
                            ? model.get(TabProperties.ACTOR_UI_STATE)
                            : null;
            if (TabListViewBinderUtils.isActorActive(actorState)) {
                // Appends "- Gemini is working on your task..." to the title when Actor is active.
                title = context.getString(R.string.tab_ax_label_actor_accessing, title);
            }

            @StringRes
            int stringRes =
                    TabListViewBinderUtils.getMediaIndicatorTabContentDescriptionStringId(
                            isPinned, mediaState);
            contentDescriptionString = context.getString(stringRes, title);
        }
        view.setContentDescription(contentDescriptionString);
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

    private static void updateChildRowPadding(PropertyModel model, View view) {
        boolean isInGroup = model.get(TabProperties.TAB_GROUP_ID) != null;
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;

        boolean isPinned = TabProperties.isPinnedTab(model);
        Context context = view.getContext();
        boolean isTablet = isTablet(context);

        int marginStart = 0;
        if (isRailCollapsed) {
            marginStart = getCollapsedChildMarginStart(context);
        } else if (isInGroup) {
            marginStart =
                    view.getResources()
                            .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
        }

        int marginBottom =
                isPinned
                        ? view.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.vertical_tab_pinned_item_margin_bottom)
                        : view.getResources()
                                .getDimensionPixelSize(
                                        isTablet
                                                ? R.dimen.vertical_tab_item_margin_bottom_tablet
                                                : R.dimen.vertical_tab_item_margin_bottom);

        if (view.getLayoutParams() instanceof ViewGroup.MarginLayoutParams params) {
            if (params.getMarginStart() != marginStart || params.bottomMargin != marginBottom) {
                params.setMarginStart(marginStart);
                params.bottomMargin = marginBottom;
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
        int railWidth =
                ViewUtils.dpToPx(context, VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        int itemWidth = getCollapsedTabItemWidth(context);
        int railStartMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_rail_horizontal_margin);
        return (railWidth - itemWidth) / 2 - railStartMargin;
    }

    private static void updateParentPadding(PropertyModel model, ViewGroup view, boolean isHeader) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        Context context = view.getContext();
        Resources resources = context.getResources();
        updateBackgroundInsets(view);
        int paddingVertical =
                resources.getDimensionPixelSize(
                        isTablet(context)
                                ? R.dimen.vertical_tab_item_padding_vertical_tablet
                                : R.dimen.vertical_tab_item_padding_vertical);
        if (isRailCollapsed) {
            view.setPadding(0, paddingVertical, 0, paddingVertical);
        } else {
            int paddingHorizontal =
                    resources.getDimensionPixelSize(R.dimen.vertical_tab_item_padding_horizontal);
            int paddingStart = isHeader ? paddingHorizontal : 0;
            view.setPaddingRelative(
                    paddingStart, paddingVertical, paddingHorizontal, paddingVertical);
        }
    }

    private static void updateBackgroundInsets(View view) {
        Context context = view.getContext();
        int targetInset =
                context.getResources()
                        .getDimensionPixelSize(
                                isTablet(context)
                                        ? R.dimen.vertical_tab_item_touch_target_inset_tablet
                                        : R.dimen.vertical_tab_item_touch_target_inset);
        Drawable currentBg = view.getBackground();
        if (currentBg == null) return;

        if (currentBg instanceof InsetDrawable insetDrawable) {
            if (targetInset == 0 && insetDrawable.getDrawable() != null) {
                view.setBackground(insetDrawable.getDrawable());
            }
        } else if (targetInset > 0) {
            view.setBackground(new InsetDrawable(currentBg, 0, targetInset, 0, targetInset));
        }
    }

    // Theme & Color Utility Methods

    /**
     * Returns whether incognito color styling should be applied for the given tab model.
     *
     * <p>Incognito colors are only applied dynamically when incognito tabs share an activity window
     * with regular tabs (e.g. foldables and phones where {@link
     * IncognitoUtils#shouldOpenIncognitoAsWindow()} is false). When incognito runs in a dedicated
     * window with an activity-level incognito theme, standard theme colors are used instead.
     */
    private static boolean isIncognito(PropertyModel model) {
        return !IncognitoUtils.shouldOpenIncognitoAsWindow()
                && model.get(TabProperties.IS_INCOGNITO);
    }

    /**
     * Resolves the background tint list for a vertical tab row based on active selection,
     * multi-selection, and incognito state.
     *
     * @param context the context to retrieve theme colors from.
     * @param isSelected whether the tab item is currently active/selected.
     * @param isMultiSelected whether the tab item is currently in the multi-selection set.
     * @param isIncognito whether incognito dark mode colors should be applied.
     * @return a {@link ColorStateList} with transparent background for unselected tabs, dark
     *     surface tint for selected incognito tabs, surface color for selected regular tabs, and
     *     multi-selected tab container tint for multi-selected tabs.
     */
    private static ColorStateList getBackgroundTintList(
            Context context, boolean isSelected, boolean isMultiSelected, boolean isIncognito) {
        if (!isSelected && !isMultiSelected) {
            return ColorStateList.valueOf(Color.TRANSPARENT);
        }
        if (isSelected) {
            int color =
                    isIncognito
                            ? context.getColor(R.color.default_bg_color_dark)
                            : SemanticColorUtils.getColorSurface(context);
            return ColorStateList.valueOf(color);
        }
        int color = TabUiThemeUtil.getTabStripMultiSelectedTabColor(context, isIncognito);
        return ColorStateList.valueOf(color);
    }

    /**
     * Resolves the title text color for a vertical tab row based on selection and incognito state.
     *
     * @param context the context to retrieve theme colors from.
     * @param isSelected whether the tab item is currently active/selected.
     * @param isIncognito whether incognito dark mode colors should be applied.
     * @return text color integer supporting high-contrast white text in incognito or theme surface
     *     text.
     */
    private static @ColorInt int getTextColor(
            Context context, boolean isSelected, boolean isIncognito) {
        if (isSelected) {
            return isIncognito
                    ? context.getColor(R.color.default_text_color_light)
                    : SemanticColorUtils.getColorOnSurface(context);
        } else {
            return isIncognito
                    ? context.getColor(R.color.incognito_tab_title_color)
                    : SemanticColorUtils.getDefaultTextColorSecondary(context);
        }
    }

    /**
     * Resolves the tint list for the action/close button based on selection and incognito state.
     *
     * @param context the context to retrieve theme colors from.
     * @param isSelected whether the tab item is currently active/selected.
     * @param isIncognito whether incognito dark mode colors should be applied.
     * @return a {@link ColorStateList} for the action button icon.
     */
    private static ColorStateList getActionButtonTintList(
            Context context, boolean isSelected, boolean isIncognito) {
        if (isIncognito) {
            return isSelected
                    ? ChromeColors.getPrimaryIconTint(context, /* isIncognito= */ true)
                    : context.getColorStateList(R.color.incognito_tab_action_button_color);
        }
        return ColorStateList.valueOf(
                isSelected
                        ? SemanticColorUtils.getDefaultIconColor(context)
                        : SemanticColorUtils.getDefaultIconColorSecondary(context));
    }

    // Gesture & Interaction Layout Helpers

    /**
     * Helper to orchestrate hover enter and exit between a parent view and an optional child
     * button. Prevents the parent from firing a visual "exit" when the mouse moves over the child
     * button.
     */
    private static void setupHoverOrchestration(
            ViewGroup parentView,
            @Nullable View childButton,
            Runnable onHoverEnter,
            Runnable onHoverExit) {
        parentView.setOnHoverListener(
                (View _, MotionEvent motionEvent) -> {
                    switch (motionEvent.getAction()) {
                        case MotionEvent.ACTION_HOVER_ENTER:
                            onHoverEnter.run();
                            return true;
                        case MotionEvent.ACTION_HOVER_EXIT:
                            float x = motionEvent.getX();
                            float y = motionEvent.getY();
                            if (x < 0
                                    || x >= parentView.getWidth()
                                    || y < 0
                                    || y >= parentView.getHeight()) {
                                onHoverExit.run();
                            }
                            return true;
                    }
                    return false;
                });

        if (childButton != null) {
            childButton.setOnHoverListener(
                    (v, motionEvent) -> {
                        int action = motionEvent.getAction();
                        if (action == MotionEvent.ACTION_HOVER_ENTER) {
                            v.setHovered(true);
                            onHoverEnter.run();
                            return true;
                        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
                            v.setHovered(false);
                            float xInView = v.getLeft() + motionEvent.getX();
                            float yInView = v.getTop() + motionEvent.getY();
                            if (xInView < 0
                                    || xInView >= parentView.getWidth()
                                    || yInView < 0
                                    || yInView >= parentView.getHeight()) {
                                onHoverExit.run();
                            }
                            return true;
                        }
                        return false;
                    });
        }
    }

    private static void applyHoverBackgroundState(
            PropertyModel model,
            ViewGroup view,
            boolean isHovered,
            @Nullable ColorStateList defaultBackgroundColor) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        boolean isMultiSelected = model.get(TabProperties.IS_MULTI_SELECTED);

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
        }

        if (isHovered) {
            // TODO(crbug.com/533531896): Handle clearing all backgrounds before
            // showing tab background.
            // TODO(crbug.com/527641177): Maybe show a darker background color for
            // action button when it's being hovered?
            if (!isSelected && !isMultiSelected) {
                ViewCompat.setBackgroundTintList(
                        view,
                        ColorStateList.valueOf(
                                TabUiThemeUtil.getHoveredTabContainerColor(
                                        view.getContext(), isIncognito(model))));
            } else if (isMultiSelected && !isSelected) {
                ViewCompat.setBackgroundTintList(
                        view,
                        ColorStateList.valueOf(
                                TabUiThemeUtil.getTabStripMultiSelectedHoveredTabColor(
                                        view.getContext(), isIncognito(model))));
            }
        } else {
            if (!isSelected) {
                ViewCompat.setBackgroundTintList(view, defaultBackgroundColor);
            }
        }
        view.invalidate();
    }

    /**
     * Configures mouse hover and keyboard focus listeners for the tab row view and optional action
     * button.
     *
     * <p>When hovered while unselected, applies {@link TabUiThemeUtil#getHoveredTabContainerColor}
     * corresponding to the current incognito state, and restores {@code defaultBackgroundColor} on
     * exit. Hover card display is triggered when either mouse hover or keyboard focus is active.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab row item.
     * @param defaultBackgroundColor the background tint list to restore on hover exit.
     */
    private static void setupTabHoverListener(
            PropertyModel model, ViewGroup view, @Nullable ColorStateList defaultBackgroundColor) {
        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);

        Runnable onHoverEnter =
                () -> {
                    TabHoverCardListener listener =
                            model.get(TabProperties.TAB_HOVER_CARD_LISTENER);
                    // Blocks new tab hover backgrounds to show when context menu or scroll occurs.
                    if (listener != null
                            && (listener.isContextMenuShowing() || listener.isScrolling())) {
                        return;
                    }
                    applyHoverBackgroundState(
                            model, view, /* isHovered= */ true, defaultBackgroundColor);
                    updateIcons(model, view, /* isHovered= */ true);
                    notifyHoverChange(model, view, /* isHovered= */ true);
                };

        Runnable onHoverExit =
                () -> {
                    applyHoverBackgroundState(
                            model, view, /* isHovered= */ false, defaultBackgroundColor);
                    updateIcons(model, view, /* isHovered= */ false);
                    notifyHoverChange(model, view, /* isHovered= */ false);
                };

        view.setTag(R.id.tab_hover_exit_listener, onHoverExit);
        setupHoverOrchestration(view, actionButton, onHoverEnter, onHoverExit);

        view.setOnFocusChangeListener((v, hasFocus) -> notifyHoverChange(model, v, hasFocus));
    }

    /**
     * Configures mouse hover and keyboard focus listeners for the tab group header row view and
     * optional menu button.
     *
     * @param model the model containing the tab group properties.
     * @param view the root ViewGroup representing the tab group header row item.
     */
    private static void setupTabGroupHeaderHoverListener(PropertyModel model, ViewGroup view) {
        @Nullable View menuButton = view.findViewById(R.id.menu_button);

        Runnable onHoverEnter =
                () -> {
                    TabHoverCardListener listener =
                            model.get(TabProperties.TAB_HOVER_CARD_LISTENER);
                    // Blocks new tab hover backgrounds to show when context menu or scroll occurs.
                    if (listener != null
                            && (listener.isContextMenuShowing() || listener.isScrolling())) {
                        return;
                    }
                    boolean isRailCollapsed =
                            model.get(TabProperties.RAIL_COLLAPSE_STATE)
                                    == RailCollapseState.COLLAPSED;
                    if (!isRailCollapsed && view.findViewById(R.id.menu_button) != null) {
                        RecordUserAction.record("Android.VerticalTabs.GroupHeaderHovered");
                    }
                    updateGroupHeaderIcons(model, view, /* isHovered= */ true);
                    notifyGroupHeaderHoverChange(model, view, /* isHovered= */ true);
                };
        Runnable onHoverExit =
                () -> {
                    updateGroupHeaderIcons(model, view, /* isHovered= */ false);
                    notifyGroupHeaderHoverChange(model, view, /* isHovered= */ false);
                };

        view.setTag(R.id.tab_hover_exit_listener, onHoverExit);
        setupHoverOrchestration(view, menuButton, onHoverEnter, onHoverExit);

        view.setOnFocusChangeListener(
                (v, hasFocus) -> notifyGroupHeaderHoverChange(model, v, hasFocus));
    }

    private static void updateGroupHeaderIcons(
            PropertyModel model, ViewGroup view, boolean isHovered) {
        boolean isRailCollapsed =
                model.get(TabProperties.RAIL_COLLAPSE_STATE) == RailCollapseState.COLLAPSED;
        View menuButton = view.findViewById(R.id.menu_button);
        if (menuButton != null) {
            menuButton.setVisibility(!isRailCollapsed && isHovered ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Notifies {@link TabHoverCardListener} of hover or keyboard focus state transitions on tab
     * items.
     */
    private static void notifyHoverChange(PropertyModel model, View view, boolean isHovered) {
        TabHoverCardListener listener = model.get(TabProperties.TAB_HOVER_CARD_LISTENER);
        if (listener != null) {
            int tabId = model.get(TabProperties.TAB_ID);
            listener.onTabHoverCardStateChanged(tabId, view, isHovered);
        }
    }

    /**
     * Notifies {@link TabHoverCardListener} of hover or keyboard focus state transitions on tab
     * group headers.
     */
    private static void notifyGroupHeaderHoverChange(
            PropertyModel model, View view, boolean isHovered) {
        TabHoverCardListener listener = model.get(TabProperties.TAB_HOVER_CARD_LISTENER);
        if (listener != null) {
            int tabId = model.get(TabProperties.TAB_ID);
            Token tabGroupId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
            if (tabGroupId == null) {
                tabGroupId = model.get(TabProperties.TAB_GROUP_ID);
            }
            listener.onTabGroupHoverCardStateChanged(tabId, tabGroupId, view, isHovered);
        }
    }
}
