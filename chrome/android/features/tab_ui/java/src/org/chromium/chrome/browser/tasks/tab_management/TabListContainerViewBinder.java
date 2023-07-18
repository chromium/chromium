// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_FOCUSED;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SHADOW_TOP_OFFSET;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabListRecyclerView.
 */
class TabListContainerViewBinder {
    /**
     * Bind the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabListRecyclerView view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            updateMargins(model, view);
            if (model.get(IS_VISIBLE)) {
                view.startShowing(model.get(ANIMATE_VISIBILITY_CHANGES));
            } else {
                view.startHiding(model.get(ANIMATE_VISIBILITY_CHANGES));
            }
        } else if (IS_INCOGNITO == propertyKey) {
            int primaryBackgroundColor = ChromeColors.getPrimaryBackgroundColor(
                    view.getContext(), model.get(IS_INCOGNITO));
            view.setBackgroundColor(primaryBackgroundColor);
            view.setToolbarHairlineColor(ThemeUtils.getToolbarHairlineColor(
                    view.getContext(), primaryBackgroundColor, model.get(IS_INCOGNITO)));
        } else if (VISIBILITY_LISTENER == propertyKey) {
            view.setVisibilityListener(model.get(VISIBILITY_LISTENER));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            int index = (Integer) model.get(INITIAL_SCROLL_INDEX);
            int offset = computeOffset(view, model);
            // RecyclerView#scrollToPosition(int) behaves incorrectly first time after cold start.
            ((LinearLayoutManager) view.getLayoutManager())
                    .scrollToPositionWithOffset(index, offset);
        } else if (TOP_MARGIN == propertyKey) {
            updateMargins(model, view);
        } else if (BOTTOM_CONTROLS_HEIGHT == propertyKey) {
            updateMargins(model, view);
        } else if (SHADOW_TOP_OFFSET == propertyKey) {
            view.setShadowTopOffset(model.get(SHADOW_TOP_OFFSET));
        } else if (BOTTOM_PADDING == propertyKey) {
            view.setBottomPadding(model.get(BOTTOM_PADDING));
        } else if (FOCUS_TAB_INDEX_FOR_ACCESSIBILITY == propertyKey) {
            int index = model.get(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);
            RecyclerView.ViewHolder selectedViewHolder =
                    view.findViewHolderForAdapterPosition(index);
            if (selectedViewHolder == null) return;
            View focusView = selectedViewHolder.itemView;
            focusView.requestFocus();
            focusView.sendAccessibilityEvent(TYPE_VIEW_FOCUSED);
        }
    }

    private static void updateMargins(PropertyModel model, TabListRecyclerView view) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
        final int oldTopMargin = params.topMargin;
        final int oldBottomMargin = params.bottomMargin;
        if (model.get(IS_VISIBLE)) {
            params.topMargin = model.get(TOP_MARGIN);
            params.bottomMargin = model.get(BOTTOM_CONTROLS_HEIGHT);
        } else {
            // Treat the bottom margin as 0 to avoid layout shift in tab shrink animations.
            // IS_VISIBLE will be set to true after the tab shrink animation see
            // {@link TabSwitcherMediator#showTabSwitcherView(boolean)}.
            params.bottomMargin = 0;

            // Leave the top margin unchanged to avoid relayouts during scrolls and for top
            // toolbar indicators while the view is not visible. Once visible the offset will
            // adjust accordingly.
        }
        if (!model.get(IS_VISIBLE)
                || (oldTopMargin == params.topMargin && oldBottomMargin == params.bottomMargin)) {
            return;
        }

        ViewUtils.requestLayout(view, "TabListContainerViewBinder.bind updateMargins");
    }

    private static int computeOffset(TabListRecyclerView view, PropertyModel model) {
        int width = view.getWidth();
        int height = view.getHeight();
        final BrowserControlsStateProvider browserControlsStateProvider =
                model.get(BROWSER_CONTROLS_STATE_PROVIDER);
        // If layout hasn't happened yet fallback to dimensions based on visible display frame. This
        // works for multi-window and different orientations. Don't use View#post() because this
        // will cause animation jank for expand/shrink animations.
        if (width == 0 || height == 0) {
            Rect frame = new Rect();
            ((Activity) view.getContext())
                    .getWindow()
                    .getDecorView()
                    .getWindowVisibleDisplayFrame(frame);
            width = frame.width();
            // Remove toolbar height from height.
            height = frame.height()
                    - Math.round(browserControlsStateProvider.getTopVisibleContentOffset());
        }
        if (width <= 0 || height <= 0) return 0;

        @TabListCoordinator.TabListMode
        int mode = model.get(MODE);
        LinearLayoutManager layoutManager = (LinearLayoutManager) view.getLayoutManager();
        if (mode == TabListCoordinator.TabListMode.GRID) {
            GridLayoutManager gridLayoutManager = (GridLayoutManager) layoutManager;
            int cardWidth = width / gridLayoutManager.getSpanCount();
            int cardHeight = TabUtils.deriveGridCardHeight(
                    cardWidth, view.getContext(), browserControlsStateProvider);
            return Math.max(0, height / 2 - cardHeight / 2);
        }
        if (mode == TabListCoordinator.TabListMode.CAROUSEL) {
            return Math.max(0,
                    width / 2
                            - view.getContext().getResources().getDimensionPixelSize(
                                      R.dimen.tab_carousel_card_width)
                                    / 2);
        }
        if (mode == TabListCoordinator.TabListMode.LIST) {
            // Avoid divide by 0 when there are no tabs.
            if (layoutManager.getItemCount() == 0) return 0;

            return Math.max(0,
                    height / 2
                            - view.computeVerticalScrollRange() / layoutManager.getItemCount() / 2);
        }
        assert false : "Unexpected MODE when setting INITIAL_SCROLL_INDEX.";
        return 0;
    }
}
