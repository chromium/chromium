// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.StyleUtils;

/** ViewBinder and ViewHolder for the Tab Search Overlay component. */
@NullMarked
public class TabSearchOverlayViewBinder {
    // This value mimics the transition duration for {@link SideUiCoordinatorImpl}.
    private static final long TRANSITION_DURATION_MS = 350L;

    /** Helper class that holds references to the underlying Android views. */
    public static class ViewHolder {
        public final View panelContainer;
        public final View panel;
        public final View emptyStateView;

        /**
         * Constructs a new ViewHolder holding the inflated views.
         *
         * @param panelContainer The root container layout for the search overlay.
         * @param panel The background overlay panel itself.
         * @param emptyStateView The empty state view.
         */
        public ViewHolder(View panelContainer, View panel, View emptyStateView) {
            this.panelContainer = panelContainer;
            this.panel = panel;
            this.emptyStateView = emptyStateView;
        }
    }

    /** Binds properties from the PropertyModel to the ViewHolder. */
    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (TabSearchOverlayProperties.EMPTY_STATE_VISIBLE == propertyKey) {
            boolean visible = model.get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE);
            view.emptyStateView.setVisibility(visible ? View.VISIBLE : View.GONE);
        } else if (TabSearchOverlayProperties.ON_CLOSE_CLICK == propertyKey) {
            view.panel
                    .findViewById(R.id.tab_search_close_button)
                    .setOnClickListener(model.get(TabSearchOverlayProperties.ON_CLOSE_CLICK));
        } else if (TabSearchOverlayProperties.ON_SCRIM_CLICK == propertyKey) {
            view.panelContainer
                    .findViewById(R.id.tab_search_overlay_scrim)
                    .setOnClickListener(model.get(TabSearchOverlayProperties.ON_SCRIM_CLICK));
        } else if (TabSearchOverlayProperties.VISIBLE == propertyKey) {
            boolean visible = model.get(TabSearchOverlayProperties.VISIBLE);
            if (visible) {
                updateCloseButton(view, model.get(TabSearchOverlayProperties.IS_INCOGNITO));
                runShowAnimation(view);
            } else {
                if (view.panelContainer.getVisibility() == View.VISIBLE) {
                    runHideAnimation(model, view);
                } else {
                    // If the overlay is already hidden (e.g. during startup or redundant hide
                    // calls), immediately set visibility to GONE instead of running the hide
                    // animation as a safeguard.
                    view.panelContainer.setVisibility(View.GONE);
                }
            }
        } else if (TabSearchOverlayProperties.IS_INCOGNITO == propertyKey) {
            boolean isIncognito = model.get(TabSearchOverlayProperties.IS_INCOGNITO);
            updateCloseButton(view, isIncognito);
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

    private static void runHideAnimation(PropertyModel model, ViewHolder view) {
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
                                    Runnable onHideFinished =
                                            model.get(TabSearchOverlayProperties.ON_HIDE_FINISHED);
                                    if (onHideFinished != null) {
                                        onHideFinished.run();
                                    }
                                }
                            }
                        })
                .start();
    }

    private static void updateCloseButton(ViewHolder view, boolean isIncognito) {
        var context = view.panel.getContext();
        ImageButton closeButton = view.panel.findViewById(R.id.tab_search_close_button);

        boolean useDesktopDensity = StyleUtils.shouldApplyDesktopDensity();

        // Configure close button layout size based on density
        int size =
                context.getResources()
                        .getDimensionPixelSize(
                                useDesktopDensity
                                        ? R.dimen.tab_search_close_button_size_desktop
                                        : R.dimen.tab_search_close_button_size);
        ViewGroup.LayoutParams layoutParams = closeButton.getLayoutParams();
        if (layoutParams.width != size || layoutParams.height != size) {
            layoutParams.width = size;
            layoutParams.height = size;
            closeButton.setLayoutParams(layoutParams);
        }

        // Set density-appropriate close button icon and background drawables.
        closeButton.setImageResource(
                useDesktopDensity
                        ? R.drawable.ic_tab_close_tabstrip_20dp
                        : R.drawable.ic_tab_close_tabstrip_24dp);
        closeButton.setBackgroundResource(
                useDesktopDensity
                        ? R.drawable.tab_close_button_bg_20dp
                        : R.drawable.tab_close_button_bg_24dp);

        // Apply profile-sensitive color tints.
        int iconTintRes =
                isIncognito
                        ? R.color.default_icon_color_light
                        : R.color.default_icon_color_tint_list;
        int bgTintRes =
                isIncognito
                        ? R.color.tab_strip_close_bg_incognito_tint_list
                        : R.color.tab_strip_close_bg_tint_list;

        closeButton.setImageTintList(AppCompatResources.getColorStateList(context, iconTintRes));
        closeButton.setBackgroundTintList(AppCompatResources.getColorStateList(context, bgTintRes));
    }
}
