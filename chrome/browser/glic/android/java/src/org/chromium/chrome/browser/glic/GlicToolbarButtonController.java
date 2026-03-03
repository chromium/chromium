// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;

import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider {
    private final Runnable mToggleGlicCallback;

    /**
     * @param context The Android context.
     * @param activeTabSupplier The currently active tab.
     * @param toggleGlicCallback Callback to run when the button is clicked to open Glic.
     */
    public GlicToolbarButtonController(
            Context context,
            Supplier<@Nullable Tab> activeTabSupplier,
            Runnable toggleGlicCallback) {
        // TODO(crbug.com/482372270): Add correct styling to button including Nudge state text,
        // active state shape change, and appropriate colors.
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp),
                context.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.GLIC,
                /* tooltipTextResId= */ Resources.ID_NULL);
        mToggleGlicCallback = toggleGlicCallback;
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        // TODO(haileywang): We should double check the tab profile and whether Glic is enabled.
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        return super.get(tab);
    }

    @Override
    public void onClick(View view) {
        mToggleGlicCallback.run();
    }
}
