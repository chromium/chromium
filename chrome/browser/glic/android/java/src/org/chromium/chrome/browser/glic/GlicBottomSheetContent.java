// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;

import java.util.List;

/**
 * Concrete implementation of {@link TabBottomSheetContent} for the Glic feature. Handles background
 * glow override and dynamic profile-based active task checking to handle Glic's suppression logic.
 */
@NullMarked
public class GlicBottomSheetContent extends TabBottomSheetContent {
    private final Profile mProfile;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     * @param backgroundColor The background color for the bottom sheet.
     * @param peekViewHeight The height of the peek view in pixels.
     * @param peekViewContainerId The resource ID for the peek view container.
     * @param onBackPressed Callback run when the back button/swipe is triggered.
     * @param profile The active user profile for actor task verification.
     */
    public GlicBottomSheetContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed,
            Profile profile) {
        super(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                onBackPressed);
        mProfile = profile;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.glic_bottom_sheet_half_height_a11y_label;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.glic_bottom_sheet_full_height_a11y_label;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.glic_bottom_sheet_closed_a11y_label;
    }

    @Override
    public @StringRes int getSheetHiddenAccessibilityStringId() {
        return R.string.glic_bottom_sheet_hidden_a11y_label;
    }

    @Override
    public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
        return new GlowSpec(
                getContentView().getContext().getColor(R.color.default_bg_color_blue),
                GlowSpec.ShadowSize.LONG);
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.glic_button_entrypoint_label);
    }

    @Override
    public boolean canBeSuppressed(BottomSheetContent nextContent) {
        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(mProfile);
        if (service == null) return true;

        List<ActorTask> activeTasks = service.getActiveTasks();
        if (activeTasks == null) return true;

        for (ActorTask task : activeTasks) {
            if (task.isUnderActorControl() && !task.isCompleted()) {
                return false; // Active agent task exists, prevent suppression.
            }
        }
        return true;
    }
}
