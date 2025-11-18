// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.ANIMATION_MANAGER;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.SCROLL_TO_POSITION;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the pinned tabs strip. */
@NullMarked
class PinnedTabStripViewBinder {
    public static void bind(PropertyModel model, RecyclerView view, PropertyKey propertyKey) {
        if (IS_VISIBLE.equals(propertyKey)) {
            PinnedTabStripAnimationManager animationManager = model.get(ANIMATION_MANAGER);
            boolean shouldBeVisible = model.get(IS_VISIBLE);
            ObservableSupplierImpl<Boolean> isVisibilityAnimationRunningSupplier =
                    model.get(IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER);
            animationManager.animatePinnedTabBarVisibility(
                    shouldBeVisible, isVisibilityAnimationRunningSupplier);
        } else if (SCROLL_TO_POSITION.equals(propertyKey)) {
            int position = model.get(SCROLL_TO_POSITION);
            if (position != -1) {
                PinnedTabStripAnimationManager animationManager = model.get(ANIMATION_MANAGER);
                // Cancel any pending visibility animations. An active visibility animation can
                // conflict with a scroll, creating a diagonal movement as tabs simultaneously
                // move down and to the left.
                animationManager.cancelPinnedTabBarAnimations(
                        model.get(IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER));

                view.scrollToPosition(position);
            }
        } else if (BACKGROUND_COLOR.equals(propertyKey)) {
            view.setBackgroundColor(model.get(BACKGROUND_COLOR));
        }
    }
}
