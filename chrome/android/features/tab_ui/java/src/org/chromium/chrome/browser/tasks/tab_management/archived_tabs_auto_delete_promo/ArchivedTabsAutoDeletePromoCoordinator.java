// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Coordinator for the "Auto Delete Archived Tabs Decision Promo" bottom sheet. This promo is shown
 * to allow users to make a one-time decision about enabling the auto-deletion of archived tabs. The
 * decision to call {@link #showPromo()} is handled externally based on appropriate trigger
 * conditions.
 */
@NullMarked
public class ArchivedTabsAutoDeletePromoCoordinator {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TabArchiveSettings mTabArchiveSettings;
    private final PropertyModel mModel;
    private SettingsNavigation mSettingsNavigation;

    private @Nullable ArchivedTabsAutoDeletePromoSheetContent mSheetContent;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable PropertyModelChangeProcessor mViewBinder;

    @IntDef({UserChoice.NONE, UserChoice.YES, UserChoice.NO})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    private @interface UserChoice {
        int NONE = 0;
        int YES = 1;
        int NO = 2;
    }

    private @UserChoice int mUserChoiceThisInstance;

    private boolean mIsSheetCurrentlyManagedByController;
    private boolean mIsFinalizedThisInstance;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param tabArchiveSettings The {@link TabArchiveSettings} instance for managing related
     *     preferences.
     */
    public ArchivedTabsAutoDeletePromoCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            TabArchiveSettings tabArchiveSettings) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabArchiveSettings = tabArchiveSettings;
        mSettingsNavigation = SettingsNavigationFactory.createSettingsNavigation();

        mModel = ArchivedTabsAutoDeletePromoProperties.createDefaultModel();

        mModel.set(
                ArchivedTabsAutoDeletePromoProperties.ON_YES_BUTTON_CLICK_LISTENER,
                (v) -> {
                    onPromoChoice(UserChoice.YES);
                });

        mModel.set(
                ArchivedTabsAutoDeletePromoProperties.ON_NO_BUTTON_CLICK_LISTENER,
                (v) -> {
                    onPromoChoice(UserChoice.NO);
                });
    }

    /** Cleans up resources. */
    public void destroy() {
        if (mIsSheetCurrentlyManagedByController && mSheetContent != null) {
            mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
            if (!mIsFinalizedThisInstance) {
                mUserChoiceThisInstance = UserChoice.NONE;
                performFinalActionsAndCleanup();
            }
        } else {
            cleanupSheetResourcesOnly();
        }
    }

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showPromo() {
        if (mIsSheetCurrentlyManagedByController) {
            return;
        }

        // In case a previous show attempt was interrupted before cleanup completed
        cleanupSheetResourcesOnly();

        String descriptionString = setPromoDescription();

        mIsFinalizedThisInstance = false;

        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.archived_tabs_auto_delete_promo, null);

        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, contentView, ArchivedTabsAutoDeletePromoViewBinder::bind);

        mSheetContent = new ArchivedTabsAutoDeletePromoSheetContent(contentView, descriptionString);

        mSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                            mUserChoiceThisInstance = UserChoice.NONE;
                        }
                        performFinalActionsAndCleanup();
                    }
                };

        if (mBottomSheetController.requestShowContent(mSheetContent, true)) {
            mBottomSheetController.addObserver(mSheetObserver);
            mIsSheetCurrentlyManagedByController = true;
        } else {
            // Failed to show. Promo was NOT seen. Clean up resources just created.
            cleanupSheetResourcesOnly();
        }
    }

    private void onPromoChoice(@UserChoice int choice) {
        mUserChoiceThisInstance = choice;
        initiateSheetDismissal(StateChangeReason.INTERACTION_COMPLETE);
    }

    /**
     * Initiates the dismissal of the bottom sheet.
     *
     * @param reason The reason for dismissal.
     */
    private void initiateSheetDismissal(@StateChangeReason int reason) {
        if (mSheetContent != null && mIsSheetCurrentlyManagedByController) {
            mBottomSheetController.hideContent(mSheetContent, true, reason);
        } else {
            if (!mIsFinalizedThisInstance) {
                performFinalActionsAndCleanup();
            }
        }
    }

    /** Sets preferences after the promo is dismissed and cleans up related sheet objects. */
    private void performFinalActionsAndCleanup() {
        if (mIsFinalizedThisInstance) {
            return;
        }
        mIsFinalizedThisInstance = true;
        boolean disableAutoDeleteFeature = mUserChoiceThisInstance == UserChoice.NO;

        mTabArchiveSettings.setAutoDeleteEnabled(true);
        mTabArchiveSettings.setAutoDeleteDecisionMade(true);
        if (disableAutoDeleteFeature) {
            mSettingsNavigation.startSettings(mContext, TabArchiveSettingsFragment.class);
        }
        if (disableAutoDeleteFeature) {
            RecordUserAction.record("Tabs.ArchivedTabAutoDeletePromo.No");
        } else {
            RecordUserAction.record("Tabs.ArchivedTabAutoDeletePromo.Yes");
        }
        cleanupSheetResourcesOnly();
    }

    /**
     * Cleans up sheet-specific resources like content, observer, and view binder, and resets
     * controller management flags.
     */
    private void cleanupSheetResourcesOnly() {
        if (mSheetObserver != null && mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mSheetObserver);
            mSheetObserver = null;
        }
        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }
        if (mViewBinder != null) {
            mViewBinder.destroy();
            mViewBinder = null;
        }
        mIsSheetCurrentlyManagedByController = false;
    }

    /* Sets and returns the auto delete delay variable in the description string. */
    private String setPromoDescription() {
        int autoDeleteTimeFrame = mTabArchiveSettings.getAutoDeleteTimeDeltaMonths();
        String descriptionString =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.archived_tabs_auto_delete_promo_description,
                                autoDeleteTimeFrame,
                                autoDeleteTimeFrame);
        mModel.set(
                ArchivedTabsAutoDeletePromoProperties.PROMO_DESCRIPTION_STRING, descriptionString);
        return descriptionString;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    boolean isSheetCurrentlyManagedForTesting() {
        return mIsSheetCurrentlyManagedByController;
    }

    void setSettingsNavigationForTesting(SettingsNavigation settingsNavigation) {
        mSettingsNavigation = settingsNavigation;
    }
}
