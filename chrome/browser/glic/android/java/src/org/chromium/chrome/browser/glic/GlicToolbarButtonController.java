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

import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider {
    // TODO(crbug.com/482372270): Add correct styling to button including Nudge state text, active
    // state shape change, and appropriate colors.
    public GlicToolbarButtonController(Context context, Supplier<@Nullable Tab> activeTabSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp),
                /* contentDescription= */ "",
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.GLIC,
                /* tooltipTextResId= */ Resources.ID_NULL);
    }

    @Override
    public void onClick(View view) {
        // TODO(crbug.com/482375066): Hook up to bottom sheet.
        Tab tab = mActiveTabSupplier.get();
        if (tab == null) return;
    }
}
