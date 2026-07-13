// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder and ViewHolder for the Tab Search Overlay component. */
@NullMarked
public class TabSearchOverlayViewBinder {
    // This value mimics the transition duration for {@link SideUiCoordinatorImpl}.
    private static final long TRANSITION_DURATION_MS = 350L;

    /** Helper class that holds references to the underlying Android views. */
    public static class ViewHolder {
        public final View panelContainer;
        public final View scrim;
        public final View panel;

        /**
         * Constructs a new ViewHolder holding the inflated views.
         *
         * @param panelContainer The root container layout for the search overlay.
         * @param scrim The background scrim view used to dismiss the overlay.
         * @param panel The background overlay panel itself.
         */
        public ViewHolder(View panelContainer, View scrim, View panel) {
            this.panelContainer = panelContainer;
            this.scrim = scrim;
            this.panel = panel;
        }
    }

    /** Binds properties from the PropertyModel to the ViewHolder. */
    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (TabSearchOverlayProperties.VISIBLE == propertyKey) {
            boolean visible = model.get(TabSearchOverlayProperties.VISIBLE);
            if (visible) {
                runShowAnimation(view);
            } else {
                if (view.panelContainer.getVisibility() == View.VISIBLE) {
                    runHideAnimation(view);
                } else {
                    // If the overlay is already hidden (e.g. during startup or redundant hide
                    // calls), immediately set visibility to GONE instead of running the hide
                    // animation as a safeguard.
                    view.panelContainer.setVisibility(View.GONE);
                }
            }
        } else if (TabSearchOverlayProperties.ON_SCRIM_CLICK == propertyKey) {
            view.scrim.setOnClickListener(model.get(TabSearchOverlayProperties.ON_SCRIM_CLICK));
        }
    }

    private static void runShowAnimation(ViewHolder view) {
        // Cancel any active animation (e.g. an ongoing hide transition) to prevent conflicts.
        view.panel.animate().cancel();
        // Make the container visible so Android can lay out and render the panel animation.
        view.panelContainer.setVisibility(View.VISIBLE);

        int width =
                view.panel.getResources().getDimensionPixelSize(R.dimen.tab_search_overlay_width);
        view.panel.setTranslationX(-width);
        view.panel
                .animate()
                .translationX(0)
                .setDuration(TRANSITION_DURATION_MS)
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                .setListener(null)
                .start();
    }

    private static void runHideAnimation(ViewHolder view) {
        // Cancel any active animation (e.g. an ongoing show transition) to prevent conflicts.
        view.panel.animate().cancel();

        int width =
                view.panel.getResources().getDimensionPixelSize(R.dimen.tab_search_overlay_width);
        view.panel
                .animate()
                .translationX(-width)
                .setDuration(TRANSITION_DURATION_MS)
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                .setListener(
                        new AnimatorListenerAdapter() {
                            private boolean mCancelled;

                            @Override
                            public void onAnimationCancel(Animator animation) {
                                mCancelled = true;
                            }

                            @Override
                            public void onAnimationEnd(Animator animation) {
                                if (!mCancelled) {
                                    view.panelContainer.setVisibility(View.GONE);
                                }
                            }
                        })
                .start();
    }
}
