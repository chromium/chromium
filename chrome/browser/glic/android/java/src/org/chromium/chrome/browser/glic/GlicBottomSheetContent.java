// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.context_sharing.R;
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
     * @param profile The active user profile for actor task verification.
     */
    public GlicBottomSheetContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            Profile profile) {
        super(contentView, fullHeightRatio, backgroundColor);
        mProfile = profile;
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
