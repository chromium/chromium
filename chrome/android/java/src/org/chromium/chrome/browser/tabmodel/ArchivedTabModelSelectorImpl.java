// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreator.NeedsTabModel;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** {@link TabModelSelector} for archived tabs. Must be instantiated and used on the UI thread. */
@NullMarked
public class ArchivedTabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    private final Profile mProfile;
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
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
    }

    @Override
    public void markTabStateInitialized() {
        if (isTabStateInitialized()) return;

        super.markTabStateInitialized();
        TabModelJniBridge model = (TabModelJniBridge) getModel(false);
        model.completeInitialization();
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this class
     * can be initialized.
     *
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    @Initializer
    @Override
    public void onNativeLibraryReady(
            TabContentManager tabContentProvider, boolean wasTabCollectionsActive) {
        assert mTabContentManager == null : "onNativeLibraryReady called twice!";

        TabCreator tabCreator = getTabCreatorManager().getTabCreator(false);
        TabModelOrderController orderController = new TabModelOrderControllerImpl(this);
        TabRemover tabRemover =
                new PassthroughTabRemover(
                        () -> {
                            TabGroupModelFilter regularFilter =
                                    getTabGroupModelFilterProvider()
                                            .getTabGroupModelFilter(/* isIncognito= */ false);
                            assumeNonNull(regularFilter);
                            return regularFilter;
                        });

        TabModelHolder normalModelHolder =
                TabModelHolderFactory.createTabModelHolder(
                        mProfile,
                        ActivityType.TABBED,
                        tabCreator,
                        // Never used.
                        /* incognitoTabCreator= */ assumeNonNull(null),
                        orderController,
                        tabContentProvider,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        this,
                        tabRemover,
                        /* supportUndo= */ true,
                        /* isArchivedTabModel= */ true,
                        ArchivedTabModelSelectorImpl::createTabUngrouper,
                        wasTabCollectionsActive);
        if (tabCreator instanceof NeedsTabModel needsTabModel) {
            needsTabModel.setTabModel(normalModelHolder.tabModel);
        }

        IncognitoTabModelHolder incognitoModelHolder =
                TabModelHolderFactory.createEmptyIncognitoTabModelHolder();

        onNativeLibraryReadyInternal(tabContentProvider, normalModelHolder, incognitoModelHolder);
    }

    @EnsuresNonNull("mTabContentManager")
    @VisibleForTesting
    void onNativeLibraryReadyInternal(
            TabContentManager tabContentProvider,
            TabModelHolder normalModelHolder,
            IncognitoTabModelHolder incognitoModelHolder) {
        mTabContentManager = tabContentProvider;
        initialize(normalModelHolder, incognitoModelHolder);

        new TabModelSelectorTabObserver(this) {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                assert false : "Tabs in the archived tab model should not be navigated.";
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null && !isReparentingInProgress()) {
                    getModel(tab.isIncognito())
                            .getTabRemover()
                            .removeTab(tab, /* allowDialog= */ false);
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
     * @param normalModelHolder The normal tab model.
     * @param incognitoModelHolder The incognito tab model.
     */
    public void initializeForTesting(
            TabModelHolder normalModelHolder, IncognitoTabModelHolder incognitoModelHolder) {
        initialize(normalModelHolder, incognitoModelHolder);
    }

    @Override
    public void selectModel(boolean incognito) {
        assert !incognito : "The archived tab model selector has no incognito mode.";
        assert !getCurrentModel().isIncognito()
                : "The incognito model of the archived tab model selector was selected.";
        // Intentional no-op.
    }

    @Override
    public void requestToShowTab(@Nullable Tab tab, @TabSelectionType int type) {
        // Intentional noop.
    }

    @Override
    public boolean isTabModelRestored() {
        return isTabStateInitialized();
    }

    private static TabUngrouper createTabUngrouper(
            boolean isIncognitoBranded, Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        return new PassthroughTabUngrouper(tabGroupModelFilterSupplier);
    }
}
