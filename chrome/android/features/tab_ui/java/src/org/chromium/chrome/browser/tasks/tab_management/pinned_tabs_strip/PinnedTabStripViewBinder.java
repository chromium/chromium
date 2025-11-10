// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.SCROLL_TO_POSITION;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the pinned tabs strip. */
@NullMarked
class PinnedTabStripViewBinder {
    private static final int FADE_ANIMATION_DURATION_MS = 400;

    public static void bind(PropertyModel model, RecyclerView view, PropertyKey propertyKey) {
        if (IS_VISIBLE.equals(propertyKey)) {
            boolean shouldBeVisible = model.get(IS_VISIBLE);
            boolean isVisible = view.getVisibility() == View.VISIBLE;
            ObservableSupplierImpl<Boolean> isVisibilityAnimationRunningSupplier =
                    model.get(IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER);

            if (shouldBeVisible && !isVisible) {
                // Animate in.
                view.post(
                        () -> {
                            view.setVisibility(View.VISIBLE);
                            view.setAlpha(0.0f);
                            view.setTranslationY(-view.getHeight());
                            view.animate()
                                    .alpha(1.0f)
                                    .translationY(0)
                                    .setDuration(FADE_ANIMATION_DURATION_MS)
                                    .withStartAction(
                                            () ->
                                                    setIsVisibilityAnimationRunning(
                                                            isVisibilityAnimationRunningSupplier,
                                                            true))
                                    .withEndAction(
                                            () -> {
                                                setIsVisibilityAnimationRunning(
                                                        isVisibilityAnimationRunningSupplier,
                                                        false);
                                            });
                        });
            } else if (!shouldBeVisible && isVisible) {
                // Animate out.
                view.animate()
                        .alpha(0.0f)
                        .translationY(-view.getHeight())
                        .withStartAction(
                                () ->
                                        setIsVisibilityAnimationRunning(
                                                isVisibilityAnimationRunningSupplier, true))
                        .withEndAction(
                                () -> {
                                    setIsVisibilityAnimationRunning(
                                            isVisibilityAnimationRunningSupplier, false);
                                    view.setVisibility(View.GONE);
                                });
            } else if (shouldBeVisible) {
                // Already visible, just ensure it's in the right state.
                view.animate()
                        .alpha(1.0f)
                        .translationY(0)
                        .withEndAction(
                                () ->
                                        setIsVisibilityAnimationRunning(
                                                isVisibilityAnimationRunningSupplier, false));
            }
        } else if (SCROLL_TO_POSITION.equals(propertyKey)) {
            int position = model.get(SCROLL_TO_POSITION);
            if (position != -1) {
                view.scrollToPosition(position);
            }
        } else if (BACKGROUND_COLOR.equals(propertyKey)) {
            view.setBackgroundColor(model.get(BACKGROUND_COLOR));
        }
    }

    private static void setIsVisibilityAnimationRunning(
            ObservableSupplierImpl<Boolean> isVisibilityAnimationRunningSupplier,
            boolean isAnimating) {
        if (isVisibilityAnimationRunningSupplier != null) {
            isVisibilityAnimationRunningSupplier.set(isAnimating);
        }
    }
}
