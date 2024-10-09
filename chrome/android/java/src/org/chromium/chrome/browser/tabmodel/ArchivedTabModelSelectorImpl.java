// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicBoolean;

/** {@link TabModelSelector} for archived tabs. Must be instantiated and used on the UI thread. */
public class ArchivedTabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    /** Flag set to false when the asynchronous loading of tabs is finished. */
    private final AtomicBoolean mSessionRestoreCompleted = new AtomicBoolean(true);

    private final Profile mProfile;
    private final TabModelOrderController mOrderController;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;

    private TabContentManager mTabContentManager;

    /**
     * Builds a {@link ArchivedTabModelSelectorImpl} instance.
     *
     * @param profile The {@link Profile} used.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param nextTabPolicySupplier A policy for next tab selection.
     * @param asyncTabParamsManager Manager of async params for reparenting.
     */
    public ArchivedTabModelSelectorImpl(
            Profile profile,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager) {
        super(tabCreatorManager, /* startIncognito= */ false);
        mProfile = profile;
        mOrderController = new TabModelOrderControllerImpl(this);
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        if (!mSessionRestoreCompleted.getAndSet(false)) return;

        // This is the first time we set
        // |mSessionRestoreCompleted|, so we need to broadcast.
        TabModelImpl model = (TabModelImpl) getModel(false);
        model.broadcastSessionRestoreComplete();
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this class
     * can be initialized.
     *
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    @Override
    public void onNativeLibraryReady(TabContentManager tabContentProvider) {
        assert mTabContentManager == null : "onNativeLibraryReady called twice!";

        TabCreator tabCreator = getTabCreatorManager().getTabCreator(false);
        // TODO(crbug.com/331688951): Consider using a custom TabModel.
        TabModelImpl normalModel =
                new TabModelImpl(
                        mProfile,
                        ActivityType.TABBED,
                        tabCreator,
                        /* incognitoTabCreator= */ null,
                        mOrderController,
                        tabContentProvider,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        this,
                        /* supportUndo= */ true,
                        /* isArchivedTabModel= */ true) {
                    @Override
                    public int index() {
                        // Intentional noop.
                        return INVALID_TAB_INDEX;
                    }

                    @Override
                    public void setIndex(int i, final @TabSelectionType int type) {
                        // Intentional noop.
                    }

                    @Override
                    public Tab getNextTabIfClosed(int id, boolean uponExit) {
                        return null;
                    }
                };
        ((ArchivedTabCreator) tabCreator).setTabModel(normalModel);

        onNativeLibraryReadyInternal(
                tabContentProvider,
                normalModel,
                EmptyTabModel.getInstance(/* isIncognito= */ true));
    }

    @VisibleForTesting
    void onNativeLibraryReadyInternal(
            TabContentManager tabContentProvider,
            TabModelInternal normalModel,
            IncognitoTabModelInternal incognitoModel) {
        mTabContentManager = tabContentProvider;
        initialize(normalModel, incognitoModel);

        new TabModelSelectorTabObserver(this) {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                assert false : "Tabs in the archived tab model should not be navigated.";
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null && !isReparentingInProgress()) {
                    getModel(tab.isIncognito()).removeTab(tab);
                }
            }
        };
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {
        assert false : "Not reached.";
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     *
     * @param normalModel The normal tab model.
     * @param incognitoModel The incognito tab model.
     */
    public void initializeForTesting(
            TabModelInternal normalModel, IncognitoTabModelInternal incognitoModel) {
        initialize(normalModel, incognitoModel);
    }

    @Override
    public void selectModel(boolean incognito) {
        assert !incognito : "The archived tab model selector has no incognito mode.";
        assert !getCurrentModel().isIncognito()
                : "The incognito model of the archived tab model selector was selected.";
        // Intentional no-op.
    }

    @Override
    public void requestToShowTab(Tab tab, @TabSelectionType int type) {
        // Intentional noop.
    }

    private void cacheTabBitmap(Tab tabToCache) {
        // Intentional noop.
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return mSessionRestoreCompleted.get();
    }
}
