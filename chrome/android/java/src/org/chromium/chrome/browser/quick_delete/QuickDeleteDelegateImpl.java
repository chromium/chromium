// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations for
 * Chrome.
 */
@NullMarked
public class QuickDeleteDelegateImpl extends QuickDeleteDelegate {
    private final Profile mProfile;
    private final Supplier<@Nullable TabSwitcher> mTabSwitcherSupplier;
    private final boolean mIsInMultiWindowMode;

    /**
     * Creates a {@link QuickDeleteController} with an archived tab model lease when available. This
     * acquires an orchestrator lease and attaches observers to {@code tabModelSelector}, so callers
     * must either call {@link QuickDeleteController#showDialog()} or {@link
     * QuickDeleteController#destroy()}.
     */
    public static QuickDeleteController createController(
            Context context,
            Profile profile,
            Supplier<@Nullable TabSwitcher> tabSwitcherSupplier,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            @Nullable LayoutManager layoutManager,
            TabModelSelector tabModelSelector) {
        Profile originalProfile = profile.getOriginalProfile();
        QuickDeleteDelegateImpl delegate =
                new QuickDeleteDelegateImpl(originalProfile, tabSwitcherSupplier);
        TabModelSelector archivedTabModelSelector = null;
        Destroyable archivedTabsLease = null;
        TabArchiveSettings archiveSettings = TabArchiveSettings.getInstance();
        if (!delegate.isInMultiWindowMode()
                && (ArchivedTabModelOrchestrator.isInstantiatedForProfile(originalProfile)
                        || !archiveSettings.hasArchivedTabCountBeenSet()
                        || archiveSettings.getArchivedTabCount() != 0)) {
            Destroyable lease =
                    ArchivedTabModelOrchestrator.acquireLease(
                            originalProfile, LeaseReason.QUICK_DELETE);
            if (lease != null) {
                ArchivedTabModelOrchestrator orchestrator =
                        ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(originalProfile);
                TabModelSelector selector =
                        orchestrator != null && orchestrator.areTabModelsInitialized()
                                ? orchestrator.getTabModelSelector()
                                : null;
                if (selector != null) {
                    archivedTabModelSelector = selector;
                    archivedTabsLease = lease;
                } else {
                    lease.destroy();
                }
            }
        }
        return new QuickDeleteController(
                context,
                delegate,
                modalDialogManager,
                snackbarManager,
                layoutManager,
                tabModelSelector,
                archivedTabModelSelector,
                archivedTabsLease);
    }

    /**
     * @param profile The original {@link Profile} that owns the data being deleted.
     * @param tabSwitcherSupplier A supplier for {@link TabSwitcher} interface that will be used to
     *     trigger the Quick Delete animation.
     */
    private QuickDeleteDelegateImpl(
            Profile profile, Supplier<@Nullable TabSwitcher> tabSwitcherSupplier) {
        assert !profile.isOffTheRecord();
        mProfile = profile;
        mTabSwitcherSupplier = tabSwitcherSupplier;
        mIsInMultiWindowMode = MultiWindowUtils.getInstanceCount(PersistedInstanceType.ANY) > 1;
    }

    @Override
    public void performQuickDelete(Runnable onDeleteFinished, @TimePeriod int timePeriod) {

        int[] dataTypes;
        if (UserPrefs.get(mProfile).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
            dataTypes =
                    new int[] {
                        BrowsingDataType.HISTORY, BrowsingDataType.SITE_DATA, BrowsingDataType.CACHE
                    };
        } else {
            dataTypes = new int[] {BrowsingDataType.SITE_DATA, BrowsingDataType.CACHE};
        }

        BrowsingDataBridge.getForProfile(mProfile)
                .clearBrowsingData(onDeleteFinished::run, dataTypes, timePeriod);
    }

    @Override
    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        TabSwitcher tabSwitcher = mTabSwitcherSupplier.get();
        if (tabSwitcher == null) {
            onAnimationEnd.run();
            return;
        }
        tabSwitcher.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    @Override
    boolean isInMultiWindowMode() {
        return mIsInMultiWindowMode;
    }
}
