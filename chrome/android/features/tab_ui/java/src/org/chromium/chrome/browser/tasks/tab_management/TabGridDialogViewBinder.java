// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.ANIMATION_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.ANIMATION_SOURCE_VIEW;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.APP_HEADER_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.BINDING_TOKEN;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.COLLAPSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.COLOR_ICON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.CONTENT_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.HAIRLINE_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.HEADER_TITLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_DIALOG_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_KEYBOARD_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_MAIN_CONTENT_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_SHARE_SHEET_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.MENU_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.PRIMARY_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SHARE_IMAGE_TILES_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SHOW_IMAGE_TILES;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SHOW_SHARE_BUTTON;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.TAB_GROUP_COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.TINT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.TITLE_CURSOR_VISIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.TITLE_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.UNGROUP_BAR_STATUS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.VISIBILITY_LISTENER;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabGridDialog. */
class TabGridDialogViewBinder {
    /** ViewHolder class to get access to all {@link View}s inside the TabGridDialog. */
    public static class ViewHolder {
        public final TabGridDialogToolbarView toolbarView;
        public final RecyclerView contentView;
        @Nullable public TabGridDialogView dialogView;

        ViewHolder(
                TabGridDialogToolbarView toolbarView,
                RecyclerView contentView,
                @Nullable TabGridDialogView dialogView) {
            this.toolbarView = toolbarView;
            this.contentView = contentView;
            this.dialogView = dialogView;
        }
    }

    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param viewHolder The ViewHolder to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        // The TabGridDialogView is effectively a singleton in the UI with multiple Mediators and
        // PropertyModel's attempting to managed it. This BINDING_TOKEN system prevents collisions.
        //
        // Only one PropertyModel should be binding to the DialogView at a time. To enforce this
        // the {@link TabGridDialogMediator} currently using the view must provide its hashCode as
        // a BINDING_TOKEN.
        final Integer bindingToken = model.get(BINDING_TOKEN);
        final Integer oldBindingToken = viewHolder.dialogView.getBindingToken();
        if (BINDING_TOKEN == propertyKey) {
            if (oldBindingToken != null && oldBindingToken.equals(bindingToken)) {
                return;
            }

            viewHolder.dialogView.setBindingToken(bindingToken);
            if (bindingToken == null) return;

            // Re-bind if the binding token ever changes.
            for (PropertyKey key : TabGridDialogProperties.ALL_KEYS) {
                if (BINDING_TOKEN == key) continue;

                bind(model, viewHolder, key);
            }
        } else if (bindingToken == null || oldBindingToken == null) {
            return;
        } else if (!bindingToken.equals(oldBindingToken)) {
            assert false
                    : "Attempting to update a property of TabGridDialog without binding token.";
            return;
        }

        // The null checks in the following blocks are there for if
        // 1) The dialogView is not initialized.
        // 2) ALL_KEYS are being re-bound upon changing BINDING_TOKEN and a value is unset in the
        //    newly bound model.
        if (COLLAPSE_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setBackButtonOnClickListener(model.get(COLLAPSE_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setNewTabButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (HEADER_TITLE == propertyKey) {
            if (model.get(HEADER_TITLE) != null) {
                viewHolder.toolbarView.setTitle(model.get(HEADER_TITLE));
            }
        } else if (CONTENT_TOP_MARGIN == propertyKey) {
            ViewGroup.LayoutParams params = viewHolder.contentView.getLayoutParams();
            if (params instanceof FrameLayout.LayoutParams) {
                ((FrameLayout.LayoutParams) params).topMargin = model.get(CONTENT_TOP_MARGIN);
                ViewUtils.requestLayout(viewHolder.contentView, "TabGridDialogViewBinder.bind");
            }
        } else if (APP_HEADER_HEIGHT == propertyKey) {
            viewHolder.dialogView.setAppHeaderHeight(model.get(APP_HEADER_HEIGHT));
        } else if (PRIMARY_COLOR == propertyKey) {
            viewHolder.toolbarView.setContentBackgroundColor(model.get(PRIMARY_COLOR));
            viewHolder.contentView.setBackgroundColor(model.get(PRIMARY_COLOR));
        } else if (TINT == propertyKey) {
            if (model.get(TINT) != null) {
                viewHolder.toolbarView.setTint(model.get(TINT));
            }
        } else if (SCRIMVIEW_CLICK_RUNNABLE == propertyKey) {
            viewHolder.dialogView.setScrimClickRunnable(model.get(SCRIMVIEW_CLICK_RUNNABLE));
        } else if (IS_DIALOG_VISIBLE == propertyKey) {
            if (model.get(IS_DIALOG_VISIBLE)) {
                viewHolder.dialogView.resetDialog(viewHolder.toolbarView, viewHolder.contentView);
                viewHolder.dialogView.showDialog();
            } else {
                viewHolder.dialogView.hideDialog();
            }
        } else if (VISIBILITY_LISTENER == propertyKey) {
            viewHolder.dialogView.setVisibilityListener(model.get(VISIBILITY_LISTENER));
        } else if (ANIMATION_SOURCE_VIEW == propertyKey) {
            viewHolder.dialogView.setupDialogAnimation(model.get(ANIMATION_SOURCE_VIEW));
        } else if (UNGROUP_BAR_STATUS == propertyKey) {
            viewHolder.dialogView.updateUngroupBar(model.get(UNGROUP_BAR_STATUS));
        } else if (DIALOG_BACKGROUND_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                int backgroundColorInt = model.get(DIALOG_BACKGROUND_COLOR);
                viewHolder.dialogView.updateDialogContainerBackgroundColor(backgroundColorInt);
                viewHolder.toolbarView.setBackgroundColorTint(backgroundColorInt);
            }
        } else if (HAIRLINE_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateHairlineColor(model.get(HAIRLINE_COLOR));
            }
        } else if (HAIRLINE_VISIBILITY == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.setHairlineVisibility(model.get(HAIRLINE_VISIBILITY));
            }
        } else if (DIALOG_UNGROUP_BAR_BACKGROUND_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarBackgroundColor(
                        model.get(DIALOG_UNGROUP_BAR_BACKGROUND_COLOR));
            }
        } else if (DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarHoveredBackgroundColor(
                        model.get(DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR));
            }
        } else if (DIALOG_UNGROUP_BAR_TEXT_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarTextColor(
                        model.get(DIALOG_UNGROUP_BAR_TEXT_COLOR));
            }
        } else if (DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarHoveredTextColor(
                        model.get(DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR));
            }
        } else if (DIALOG_UNGROUP_BAR_TEXT == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarText(model.get(DIALOG_UNGROUP_BAR_TEXT));
            }
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            if (model.get(INITIAL_SCROLL_INDEX) != null) {
                int index = (Integer) model.get(INITIAL_SCROLL_INDEX);
                RecyclerView view = viewHolder.contentView;
                if (view.getWidth() == 0 || view.getHeight() == 0) {
                    // If layout hasn't happened post the scroll index change until layout happens.
                    view.post(
                            () -> {
                                setScrollIndex(
                                        model.get(BROWSER_CONTROLS_STATE_PROVIDER), view, index);
                            });
                    return;
                }
                setScrollIndex(
                        model.get(BROWSER_CONTROLS_STATE_PROVIDER), viewHolder.contentView, index);
            }
        } else if (IS_MAIN_CONTENT_VISIBLE == propertyKey) {
            viewHolder.contentView.setVisibility(View.VISIBLE);
        } else if (MENU_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setMenuButtonOnClickListener(model.get(MENU_CLICK_LISTENER));
        } else if (TITLE_TEXT_WATCHER == propertyKey) {
            if (model.get(TITLE_TEXT_WATCHER) != null) {
                viewHolder.toolbarView.setTitleTextOnChangedListener(model.get(TITLE_TEXT_WATCHER));
            }
        } else if (TITLE_TEXT_ON_FOCUS_LISTENER == propertyKey) {
            if (model.get(TITLE_TEXT_ON_FOCUS_LISTENER) != null) {
                viewHolder.toolbarView.setTitleTextOnFocusChangeListener(
                        model.get(TITLE_TEXT_ON_FOCUS_LISTENER));
            }
        } else if (TITLE_CURSOR_VISIBILITY == propertyKey) {
            viewHolder.toolbarView.setTitleCursorVisibility(model.get(TITLE_CURSOR_VISIBILITY));
        } else if (IS_TITLE_TEXT_FOCUSED == propertyKey) {
            viewHolder.toolbarView.updateTitleTextFocus(model.get(IS_TITLE_TEXT_FOCUSED));
        } else if (IS_KEYBOARD_VISIBLE == propertyKey) {
            viewHolder.toolbarView.updateKeyboardVisibility(model.get(IS_KEYBOARD_VISIBLE));
        } else if (COLLAPSE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            viewHolder.toolbarView.setBackButtonContentDescription(
                    model.get(COLLAPSE_BUTTON_CONTENT_DESCRIPTION));
        } else if (SHARE_BUTTON_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setShareButtonClickListener(
                    model.get(SHARE_BUTTON_CLICK_LISTENER));
        } else if (SHARE_IMAGE_TILES_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setImageTilesClickListener(
                    model.get(SHARE_IMAGE_TILES_CLICK_LISTENER));
        } else if (SHOW_SHARE_BUTTON == propertyKey) {
            viewHolder.toolbarView.setShareButtonVisibility(model.get(SHOW_SHARE_BUTTON));
        } else if (SHOW_IMAGE_TILES == propertyKey) {
            viewHolder.toolbarView.setImageTilesVisibility(model.get(SHOW_IMAGE_TILES));
        } else if (TAB_GROUP_COLOR_ID == propertyKey) {
            viewHolder.toolbarView.setColorIconColor(
                    model.get(TAB_GROUP_COLOR_ID), model.get(IS_INCOGNITO));
        } else if (COLOR_ICON_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setColorIconOnClickListener(
                    model.get(COLOR_ICON_CLICK_LISTENER));
        } else if (IS_SHARE_SHEET_VISIBLE == propertyKey) {
            if (!model.get(IS_SHARE_SHEET_VISIBLE) && model.get(IS_DIALOG_VISIBLE)) {
                // Fit the scrim to the TabGridDialog again after the bottom sheet visibility
                // changes.
                viewHolder.dialogView.refreshScrim();
            }
        } else if (ANIMATION_BACKGROUND_COLOR == propertyKey) {
            // Only set in LIST mode not GRID mode. Will always be set in LIST mode. Mode is not
            // mutable without restarting the app.
            if (model.get(ANIMATION_BACKGROUND_COLOR) != null) {
                viewHolder.dialogView.updateAnimationBackgroundColor(
                        model.get(ANIMATION_BACKGROUND_COLOR));
            }
        } else if (FORCE_ANIMATION_TO_FINISH == propertyKey) {
            if (model.get(FORCE_ANIMATION_TO_FINISH)) {
                viewHolder.dialogView.forceAnimationToFinish();
            }
        }
    }

    private static void setScrollIndex(
            BrowserControlsStateProvider browserControlsStateProvider,
            RecyclerView view,
            int index) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) view.getLayoutManager();
        int offset = computeOffset(view, layoutManager, browserControlsStateProvider);
        layoutManager.scrollToPositionWithOffset(index, offset);
    }

    private static int computeOffset(
            RecyclerView view,
            LinearLayoutManager layoutManager,
            BrowserControlsStateProvider browserControlsStateProvider) {
        int width = view.getWidth();
        int height = view.getHeight();
        int cardHeight = 0;
        if (layoutManager instanceof GridLayoutManager) {
            int cardWidth = width / ((GridLayoutManager) layoutManager).getSpanCount();
            cardHeight =
                    TabUtils.deriveGridCardHeight(
                            cardWidth, view.getContext(), browserControlsStateProvider);
        } else {
            // Avoid divide by 0 when there are no tabs.
            if (layoutManager.getItemCount() == 0) return 0;

            cardHeight = view.computeVerticalScrollRange() / layoutManager.getItemCount();
        }
        return Math.max(0, height / 2 - cardHeight / 2);
    }
}
