// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SHADOW_TOP_OFFSET;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;

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
            if (model.get(IS_VISIBLE)) {
                view.startShowing(model.get(ANIMATE_VISIBILITY_CHANGES));
            } else {
                view.startHiding(model.get(ANIMATE_VISIBILITY_CHANGES));
            }
        } else if (IS_INCOGNITO == propertyKey) {
            view.setBackgroundColor(ChromeColors.getPrimaryBackgroundColor(
                    view.getContext(), model.get(IS_INCOGNITO)));
        } else if (VISIBILITY_LISTENER == propertyKey) {
            view.setVisibilityListener(model.get(VISIBILITY_LISTENER));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            // RecyclerView#scrollToPosition(int) behaves incorrectly first time after cold start.
            int index = (Integer) model.get(INITIAL_SCROLL_INDEX);
            ((LinearLayoutManager) view.getLayoutManager()).scrollToPositionWithOffset(index, 0);
        } else if (TOP_MARGIN == propertyKey) {
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
            final int newTopMargin = model.get(TOP_MARGIN);
            if (newTopMargin == params.topMargin) return;

            params.topMargin = newTopMargin;
            ViewUtils.requestLayout(view, "TabListContainerViewBinder.bind TOP_MARGIN");
        } else if (BOTTOM_CONTROLS_HEIGHT == propertyKey) {
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) view.getLayoutParams();
            params.bottomMargin = model.get(BOTTOM_CONTROLS_HEIGHT);
            ViewUtils.requestLayout(view, "TabListContainerViewBinder.bind BOTTOM_CONTROLS_HEIGHT");
        } else if (SHADOW_TOP_OFFSET == propertyKey) {
            view.setShadowTopOffset(model.get(SHADOW_TOP_OFFSET));
        } else if (BOTTOM_PADDING == propertyKey) {
            view.setBottomPadding(model.get(BOTTOM_PADDING));
        }
    }
}
