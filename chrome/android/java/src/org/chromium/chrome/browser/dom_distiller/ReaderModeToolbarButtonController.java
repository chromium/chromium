// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Responsible for providing UI resources for showing a reader mode button on toolbar. */
public class ReaderModeToolbarButtonController extends BaseButtonDataProvider {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabSupplierObserver mActivityTabObserver;
    private final ButtonSpec mEntryPointSpec;
    private final ButtonSpec mExitPointSpec;

    /**
     * Creates a new instance of {@code ReaderModeToolbarButtonController}.
     *
     * @param context The context for retrieving string resources.
     * @param activityTabSupplier Supplier for the current active tab.
     * @param modalDialogManager Modal dialog manager, used to disable the button when a dialog is
     *     visible. Can be null to disable this behavior.
     */
    public ReaderModeToolbarButtonController(
            Context context,
            ActivityTabProvider activityTabProvider,
            ModalDialogManager modalDialogManager) {
        super(
                activityTabProvider,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_mobile_friendly_24dp),
                context.getString(R.string.show_reading_mode_text),
                /* actionChipLabelResId= */ R.string.show_reading_mode_text,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.READER_MODE,
                /* tooltipTextResId= */ Resources.ID_NULL);

        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mActivityTabObserver =
                new TabSupplierObserver(mActivityTabProvider) {
                    @Override
                    public void onUrlUpdated(@Nullable Tab tab) {
                        maybeRefreshButton();
                    }

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        maybeRefreshButton();
                    }
                };

        mEntryPointSpec = mButtonData.getButtonSpec();
        Drawable exitPointDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_mobile_friendly_24dp);
        exitPointDrawable.setColorFilter(
                new PorterDuffColorFilter(
                        SemanticColorUtils.getDefaultIconColorAccent1(mContext),
                        PorterDuff.Mode.SRC_ATOP));
        mExitPointSpec =
                new ButtonSpec(
                        exitPointDrawable,
                        /* onClickListener= */ this,
                        /* onLongClickListener= */ null,
                        /* contentDescription= */ context.getString(
                                R.string.hide_reading_mode_text),
                        /* supportsTinting= */ true,
                        /* iphCommandBuilder= */ null,
                        AdaptiveToolbarButtonVariant.READER_MODE,
                        /* actionChipLabelResId= */ R.string.hide_reading_mode_text,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* hasErrorBadge= */ false);
    }

    @Override
    public void destroy() {
        mActivityTabObserver.destroy();
        super.destroy();
    }

    @Override
    public void onClick(View view) {
        Tab currentTab = mActiveTabSupplier.get();
        if (currentTab == null) return;

        // Note: Hidden behind feature flag.
        if (DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()
                && DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl())) {
            currentTab.goBack();
            return;
        }

        ReaderModeManager readerModeManager =
                currentTab.getUserDataHost().getUserData(ReaderModeManager.class);
        if (readerModeManager == null) return;

        readerModeManager.activateReaderMode();
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        IphCommandBuilder iphCommandBuilder =
                new IphCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.reader_mode_message_title,
                        /* accessibilityStringId= */ R.string.show_reading_mode_text);
        return iphCommandBuilder;
    }

    private void maybeRefreshButton() {
        if (!DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()) return;

        Tab tab = mActivityTabProvider.get();
        if (tab != null && DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
            mButtonData.setButtonSpec(mExitPointSpec);
        } else {
            mButtonData.setButtonSpec(mEntryPointSpec);
        }

        notifyObservers(mButtonData.canShow());
    }

    // Testing-specific functions

    TabSupplierObserver getTabSupplierObserverForTesting() {
        return mActivityTabObserver;
    }

    ButtonData getButtonDataForTesting() {
        return mButtonData;
    }
}
