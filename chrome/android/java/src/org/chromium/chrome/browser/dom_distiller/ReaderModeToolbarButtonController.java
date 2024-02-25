// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Responsible for providing UI resources for showing a reader mode button on toolbar. */
public class ReaderModeToolbarButtonController extends BaseButtonDataProvider {
    /**
     * Creates a new instance of {@code ReaderModeToolbarButtonController}.
     *
     * @param context The context for retrieving string resources.
     * @param activeTabSupplier Supplier for the current active tab.
     * @param modalDialogManager Modal dialog manager, used to disable the button when a dialog is
     * visible. Can be null to disable this behavior.
     * @param buttonDrawable Drawable for the button icon.
     */
    public ReaderModeToolbarButtonController(
            Context context,
            Supplier<Tab> activeTabSupplier,
            ModalDialogManager modalDialogManager,
            Drawable buttonDrawable) {
        super(
                activeTabSupplier,
                modalDialogManager,
                buttonDrawable,
                context.getString(R.string.reader_view_text_alt),
                /* actionChipLabelResId= */ R.string.reader_mode_action_chip_label_simplify_page,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.READER_MODE,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);
    }

    @Override
    public void onClick(View view) {
        Tab currentTab = mActiveTabSupplier.get();
        if (currentTab == null) return;

        ReaderModeManager readerModeManager =
                currentTab.getUserDataHost().getUserData(ReaderModeManager.class);
        if (readerModeManager == null) return;

        readerModeManager.activateReaderMode();
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.reader_mode_message_title,
                        /* accessibilityStringId= */ R.string.reader_view_text_alt);
        return iphCommandBuilder;
    }
}
