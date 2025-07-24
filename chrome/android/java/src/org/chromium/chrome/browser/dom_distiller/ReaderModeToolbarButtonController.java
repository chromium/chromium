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

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.Objects;

/** Responsible for providing UI resources for showing a reader mode button on toolbar. */
@NullMarked
public class ReaderModeToolbarButtonController extends BaseButtonDataProvider {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabSupplierObserver mActivityTabObserver;
    private final ButtonSpec mEntryPointSpec;
    private final ButtonSpec mExitPointSpec;
    private final BottomSheetController mBottomSheetController;
    // Created as needed.
    private @Nullable ReaderModeBottomSheetCoordinator mReaderModeBottomSheetCoordinator;
    // Only populated when the TabSupplierObserver events fire.
    private @Nullable GURL mTabLastUrlSeen;

    /**
     * Creates a new instance of {@code ReaderModeToolbarButtonController}.
     *
     * @param context The context for retrieving string resources.
     * @param profileSupplier Supplies the current profile.
     * @param activityTabSupplier Supplier for the current active tab.
     * @param modalDialogManager Modal dialog manager, used to disable the button when a dialog is
     *     visible. Can be null to disable this behavior.
     * @param bottomSheetController The bottom sheet controller, used to show the reader mode bottom
     *     sheet.
     */
    public ReaderModeToolbarButtonController(
            Context context,
            ObservableSupplier<Profile> profileSupplier,
            ActivityTabProvider activityTabProvider,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController) {
        super(
                activityTabProvider,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_mobile_friendly_24dp),
                context.getString(R.string.reader_mode_cpa_button_text),
                /* actionChipLabelResId= */ R.string.reader_mode_cpa_button_text,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.READER_MODE,
                /* tooltipTextResId= */ Resources.ID_NULL);

        mContext = context;
        mProfileSupplier = profileSupplier;
        mActivityTabProvider = activityTabProvider;
        mBottomSheetController = bottomSheetController;
        mActivityTabObserver =
                new TabSupplierObserver(mActivityTabProvider) {
                    @Override
                    public void onUrlUpdated(@Nullable Tab tab) {
                        maybeRefreshButton(tab);

                        GURL currentUrl = tab == null ? null : tab.getUrl();
                        if (Objects.equals(currentUrl, mTabLastUrlSeen)) return;
                        mTabLastUrlSeen = currentUrl;
                        maybeShowBottomSheet(tab);
                    }

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        maybeRefreshButton(tab);
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
        if (mReaderModeBottomSheetCoordinator != null) {
            mReaderModeBottomSheetCoordinator.destroy();
        }
        super.destroy();
    }

    @Override
    public void onClick(View view) {
        Tab currentTab = mActiveTabSupplier.get();
        if (currentTab == null) return;

        ReaderModeManager readerModeManager =
                currentTab.getUserDataHost().getUserData(ReaderModeManager.class);
        if (readerModeManager == null) return;

        // Note: Hidden behind feature flag.
        if (DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()
                && DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl())) {
            readerModeManager.hideReaderMode();
            return;
        }

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

    private void maybeRefreshButton(@Nullable Tab tab) {
        if (!DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()) return;

        if (tab != null && DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
            mButtonData.setButtonSpec(mExitPointSpec);
        } else {
            mButtonData.setButtonSpec(mEntryPointSpec);
        }

        notifyObservers(mButtonData.canShow());
    }

    private void maybeShowBottomSheet(@Nullable Tab tab) {
        if (!DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()) return;
        if (tab == null || !DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) return;

        if (mReaderModeBottomSheetCoordinator == null) {
            mReaderModeBottomSheetCoordinator =
                    new ReaderModeBottomSheetCoordinator(
                            mContext, mProfileSupplier.get(), mBottomSheetController);
        }
        mReaderModeBottomSheetCoordinator.show();
    }

    // Testing-specific functions

    TabSupplierObserver getTabSupplierObserverForTesting() {
        return mActivityTabObserver;
    }

    ButtonData getButtonDataForTesting() {
        return mButtonData;
    }
}
