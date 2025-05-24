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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Controller for the Read Aloud button in the top toolbar. */
@NullMarked
public class ReadAloudToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<@Nullable ReadAloudController> mControllerSupplier;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;

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
            Supplier<@Nullable Tab> activeTabSupplier,
            Drawable buttonDrawable,
            Supplier<@Nullable ReadAloudController> controllerSupplier,
            Supplier<@Nullable Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                context.getString(R.string.menu_listen_to_this_page),
                Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.READ_ALOUD,
                /* tooltipTextResId= */ Resources.ID_NULL);
        mControllerSupplier = controllerSupplier;
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    public void onClick(View view) {
        Tab tab = mActiveTabSupplier.get();
        ReadAloudController controller = mControllerSupplier.get();
        if (controller == null || tab == null) {
            return;
        }

        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED);
        }

        RecordUserAction.record("MobileTopToolbarReadAloudButton");
        controller.playTab(tab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IphCommandBuilder(
                tab.getContext().getResources(),
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_READ_ALOUD_FEATURE,
                /* stringId= */ R.string.adaptive_toolbar_button_read_aloud_iph,
                /* accessibilityStringId= */ R.string.adaptive_toolbar_button_read_aloud_iph);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        ReadAloudController controller = mControllerSupplier.get();
        if (!super.shouldShowButton(tab) || tab == null || controller == null) {
            return false;
        }
        return controller.isReadable(tab);
    }
}
