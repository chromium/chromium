// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.ui.lens.LensOverlayInvocationSource;

import java.util.function.Supplier;

/** Handles the "Ask Google" button on the adaptive toolbar. */
@NullMarked
public class AskGoogleToolbarButtonController extends BaseButtonDataProvider {
    /**
     * Creates a new instance of {@link AskGoogleToolbarButtonController}.
     *
     * @param activeTabSupplier Supplier for the current active tab.
     * @param buttonDrawable Drawable for the button icon.
     * @param contentDescription String for the button's content description.
     */
    public AskGoogleToolbarButtonController(
            Supplier<@Nullable Tab> activeTabSupplier,
            Drawable buttonDrawable,
            String contentDescription) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                new ButtonSpec.Builder(
                                buttonDrawable, contentDescription, /* supportsTinting= */ true)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.ASK_GOOGLE)
                        .build());
    }

    @Override
    public void onClick(View view) {
        Tab tab = mActiveTabSupplier.get();
        if (tab == null) return;

        int invocationSource =
                AdaptiveToolbarFeatures.isAskGoogleForcedForAdaptiveToolbar()
                        ? LensOverlayInvocationSource.COBROWSE_TOOLBAR_BUTTON
                        : LensOverlayInvocationSource.COBROWSE_PINNED_TOOLBAR_BUTTON;
        ContextualTasksUtils.toggleAskGoogleSidePanel(tab, invocationSource);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        return super.shouldShowButton(tab)
                && ContextualTasksUtils.shouldShowAskGoogleEntryPoint(
                        tab, ChromeFeatureList.sContextualTasksEntryPointsToolbarButton);
    }
}
