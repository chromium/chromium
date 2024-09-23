// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Controller for the Read Aloud button in the top toolbar. */
public class ReadAloudToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<ReadAloudController> mControllerSupplier;
    private final Supplier<Tracker> mTrackerSupplier;

    /**
     * Creates a new instance of {@code TranslateButtonController}.
     *
     * @param context            The Context for retrieving string resources.
     * @param activeTabSupplier  Supplier for the current active tab.
     * @param buttonDrawable     Drawable for the button icon.
     * @param controllerSupplier Supplier for the ReadAloud feature controller.
     * @param trackerSupplier    Supplier for the IPH.
     */
    public ReadAloudToolbarButtonController(
            Context context,
            Supplier<Tab> activeTabSupplier,
            Drawable buttonDrawable,
            Supplier<ReadAloudController> controllerSupplier,
            Supplier<Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                context.getString(R.string.menu_listen_to_this_page),
                Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.READ_ALOUD,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ true);
        mControllerSupplier = controllerSupplier;
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    public void onClick(View view) {
        if (!mControllerSupplier.hasValue() || !mActiveTabSupplier.hasValue()) {
            return;
        }

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED);
        }

        RecordUserAction.record("MobileTopToolbarReadAloudButton");
        mControllerSupplier
                .get()
                .playTab(mActiveTabSupplier.get(), ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IPHCommandBuilder(
                tab.getContext().getResources(),
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_READ_ALOUD_FEATURE,
                /* stringId= */ R.string.adaptive_toolbar_button_read_aloud_iph,
                /* accessibilityStringId= */ R.string.adaptive_toolbar_button_read_aloud_iph);
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (!super.shouldShowButton(tab) || tab == null || mControllerSupplier.get() == null) {
            return false;
        }
        return mControllerSupplier.get().isReadable(tab);
    }
}
