// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the pinned tabs strip. */
@NullMarked
class PinnedTabStripViewBinder {
    public static void bind(PropertyModel model, RecyclerView view, PropertyKey propertyKey) {
        if (PinnedTabStripProperties.IS_VISIBLE.equals(propertyKey)) {
            boolean shouldBeVisible = model.get(PinnedTabStripProperties.IS_VISIBLE);
            boolean isVisible = view.getVisibility() == View.VISIBLE;

            if (shouldBeVisible && !isVisible) {
                // Animate in.
                view.post(
                        () -> {
                            view.setAlpha(0.0f);
                            view.setTranslationY(-view.getHeight());
                            view.setVisibility(View.VISIBLE);
                            view.animate().alpha(1.0f).translationY(0).withEndAction(null);
                        });
            } else if (!shouldBeVisible && isVisible) {
                // Animate out.
                view.animate()
                        .alpha(0.0f)
                        .translationY(-view.getHeight())
                        .withEndAction(() -> view.setVisibility(View.GONE));
            } else if (shouldBeVisible) {
                // Already visible, just ensure it's in the right state.
                view.animate().alpha(1.0f).translationY(0).withEndAction(null);
            }
        } else if (PinnedTabStripProperties.SCROLL_TO_POSITION.equals(propertyKey)) {
            int position = model.get(PinnedTabStripProperties.SCROLL_TO_POSITION);
            if (position != -1) {
                view.scrollToPosition(position);
            }
        }
    }
}
