// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class manages all the ContentViews in the app.  As it manipulates views, it must be
 * instantiated and used in the UI Thread.  It acts as a TabModel which delegates all
 * TabModel methods to the active model that it contains.
 */
public class TabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    public static final int CUSTOM_TABS_SELECTOR_INDEX = -1;

    /** Flag set to false when the asynchronous loading of tabs is finished. */
    private final AtomicBoolean mSessionRestoreInProgress =
            new AtomicBoolean(true);
    private final TabPersistentStore mTabSaver;

    private boolean mIsUndoSupported;

    // Whether the Activity that owns that TabModelSelector is tabbed or not.
    // Used by sync to determine how to handle restore on cold start.
    private boolean mIsTabbedActivityForSync;

    private final TabModelOrderController mOrderController;

    private final AsyncTabParamsManager mAsyncTabParamsManager;

    private NextTabPolicySupplier mNextTabPolicySupplier;

    private TabContentManager mTabContentManager;

    private Tab mVisibleTab;

    private CloseAllTabsDelegate mCloseAllTabsDelegate;

    private final Supplier<WindowAndroid> mWindowAndroidSupplier;

    /**
     * Builds a {@link TabModelSelectorImpl} instance.
     * @param activity An {@link Activity} instance.
     * @param windowAndroidSupplier A supplier of {@link WindowAndroid} instance which is passed
     *         down to {@link IncognitoTabModelImplCreator} for creating {@link IncognitoTabModel}.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param persistencePolicy A {@link TabPersistencePolicy} instance.
     * @param tabModelFilterFactory
     * @param nextTabPolicySupplier
     * @param asyncTabParamsManager
     * @param supportUndo Whether a tab closure can be undone.
     */
    public TabModelSelectorImpl(Activity activity,
            @Nullable Supplier<WindowAndroid> windowAndroidSupplier,
            TabCreatorManager tabCreatorManager, TabPersistencePolicy persistencePolicy,
            TabModelFilterFactory tabModelFilterFactory,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager, boolean supportUndo,
            boolean isTabbedActivity, boolean startIncognito) {
        super(tabCreatorManager, tabModelFilterFactory, startIncognito);
        mWindowAndroidSupplier = windowAndroidSupplier;
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
            @Override
            public void onStateLoaded() {
                markTabStateInitialized();
            }
        };
        mIsUndoSupported = supportUndo;
        mIsTabbedActivityForSync = isTabbedActivity;
        mTabSaver = new TabPersistentStore(persistencePolicy, this, tabCreatorManager);
        mTabSaver.addObserver(persistentStoreObserver);
        mOrderController = new TabModelOrderControllerImpl(this);
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        if (!mSessionRestoreInProgress.getAndSet(false)) return;

        // This is the first time we set
        // |mSessionRestoreInProgress|, so we need to broadcast.
        TabModelImpl model = (TabModelImpl) getModel(false);

        if (model != null) {
            model.broadcastSessionRestoreComplete();
        } else {
            assert false : "Normal tab model is null after tab state loaded.";
        }
    }

    private void handleOnPageLoadStopped(Tab tab) {
        if (tab != null) mTabSaver.addTabToSaveQueue(tab);
    }

    /**
     * Should be called when the app starts showing a view with multiple tabs.
     */
    public void onTabsViewShown() {
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this
     * class can be initialized.
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    public void onNativeLibraryReady(TabContentManager tabContentProvider) {
        assert mTabContentManager == null : "onNativeLibraryReady called twice!";

        ChromeTabCreator regularTabCreator =
                (ChromeTabCreator) getTabCreatorManager().getTabCreator(false);
        ChromeTabCreator incognitoTabCreator =
                (ChromeTabCreator) getTabCreatorManager().getTabCreator(true);
        TabModelImpl normalModel = new TabModelImpl(Profile.getLastUsedRegularProfile(),
                mIsTabbedActivityForSync, regularTabCreator, incognitoTabCreator, mOrderController,
                mTabContentManager, mTabSaver, mNextTabPolicySupplier, mAsyncTabParamsManager, this,
                mIsUndoSupported);
        regularTabCreator.setTabModel(normalModel, mOrderController);

        IncognitoTabModel incognitoModel = new IncognitoTabModelImpl(
                new IncognitoTabModelImplCreator(mWindowAndroidSupplier, regularTabCreator,
                        incognitoTabCreator, mOrderController, mTabContentManager, mTabSaver,
                        mNextTabPolicySupplier, mAsyncTabParamsManager, this));
        incognitoTabCreator.setTabModel(incognitoModel, mOrderController);
        onNativeLibraryReadyInternal(tabContentProvider, normalModel, incognitoModel);
    }

    @VisibleForTesting
    void onNativeLibraryReadyInternal(TabContentManager tabContentProvider, TabModel normalModel,
            IncognitoTabModel incognitoModel) {
        mTabContentManager = tabContentProvider;
        initialize(normalModel, incognitoModel);
        mTabSaver.setTabContentManager(mTabContentManager);

        addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                // Only invalidate if the tab exists in the currently selected model.
                if (TabModelUtils.getTabById(getCurrentModel(), tab.getId()) != null) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrlString());
                }

                if (creationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
                    mTabSaver.addTabToSaveQueue(tab);
                }
            }
        });

        new TabModelSelectorTabObserver(this) {
            @Override
            public void onUrlUpdated(Tab tab) {
                TabModel model = getModelForTabId(tab.getId());
                if (model == getCurrentModel()) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrlString());
                }
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                handleOnPageLoadStopped(tab);
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                String previousUrl = tab.getUrlString();
                mTabContentManager.invalidateTabThumbnail(tab.getId(), previousUrl);
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                tab.getId();
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                tab.getId();
            }

            @Override
            public void onCrash(Tab tab) {
                if (SadTab.isShowing(tab)) mTabContentManager.removeTabThumbnail(tab.getId());
                tab.getId();
            }

            @Override
            public void onNavigationEntriesDeleted(Tab tab) {
                mTabSaver.addTabToSaveQueue(tab);
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null && !isReparentingInProgress()) {
                    getModel(tab.isIncognito()).removeTab(tab);
                }
            }

            @Override
            public void onCloseContents(Tab tab) {
                closeTab(tab);
            }

            @Override
            public void onRootIdChanged(Tab tab, int newRootId) {
                mTabSaver.addTabToSaveQueue(tab);
            }
        };
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     * @param normalModel The normal tab model.
     * @param incognitoModel The incognito tab model.
     */
    @VisibleForTesting
    public void initializeForTesting(TabModel normalModel, IncognitoTabModel incognitoModel) {
        initialize(normalModel, incognitoModel);
    }

    @Override
    public void setCloseAllTabsDelegate(CloseAllTabsDelegate delegate) {
        mCloseAllTabsDelegate = delegate;
    }

    @Override
    public void selectModel(boolean incognito) {
        TabModel oldModel = getCurrentModel();
        super.selectModel(incognito);
        TabModel newModel = getCurrentModel();
        if (oldModel != newModel) {
            TabModelUtils.setIndex(newModel, newModel.index());

            // Make the call to notifyDataSetChanged() after any delayed events
            // have had a chance to fire. Otherwise, this may result in some
            // drawing to occur before animations have a chance to work.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    notifyChanged();
                }
            });
        }
    }

    /**
     * Commits all pending tab closures for all {@link TabModel}s in this {@link TabModelSelector}.
     */
    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < getModels().size(); i++) {
            getModels().get(i).commitAllTabClosures();
        }
    }

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        return mCloseAllTabsDelegate.closeAllTabsRequest(incognito);
    }

    /**
     * Save the current state of the tab model. Usage of this method is discouraged due to it
     * writing to disk.
     */
    public void saveState() {
        commitAllTabClosures();
        mTabSaver.saveState();
    }

    /**
     * Load the saved tab state. This should be called before any new tabs are created. The saved
     * tabs shall not be restored until {@link #restoreTabs} is called.
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     */
    public void loadState(boolean ignoreIncognitoFiles) {
        mTabSaver.loadState(ignoreIncognitoFiles);
    }

    @Override
    public void mergeState() {
        mTabSaver.mergeState();
    }

    /**
     * Restore the saved tabs which were loaded by {@link #loadState}.
     *
     * @param setActiveTab If true, synchronously load saved active tab and set it as the current
     *                     active tab.
     */
    public void restoreTabs(boolean setActiveTab) {
        mTabSaver.restoreTabs(setActiveTab);
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given url as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this url, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForUrl(String url) {
        if (isSessionRestoreInProgress()) mTabSaver.restoreTabStateForUrl(url);
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given id as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this id, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForId(int id) {
        if (isSessionRestoreInProgress()) mTabSaver.restoreTabStateForId(id);
    }

    public void clearState() {
        mTabSaver.clearState();
    }

    @Override
    public void destroy() {
        mTabSaver.destroy();
        super.destroy();
    }

    /**
     * @return Number of restored tabs on cold startup.
     */
    public int getRestoredTabCount() {
        return mTabSaver.getRestoredTabCount();
    }

    @Override
    public void requestToShowTab(Tab tab, @TabSelectionType int type) {
        boolean isFromExternalApp =
                tab != null && tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
        if (mVisibleTab != tab && tab != null && !tab.isNativePage()) {
            TabSwitchMetrics.startTabSwitchLatencyTiming(type);
        }
        if (mVisibleTab != null && mVisibleTab != tab && !mVisibleTab.needsReload()) {
            boolean attached = mVisibleTab.getWebContents() != null
                    && mVisibleTab.getWebContents().getTopLevelNativeWindow() != null;
            if (mVisibleTab.isInitialized() && attached) {
                // TODO(dtrainor): Once we figure out why we can't grab a snapshot from the current
                // tab when we have other tabs loading from external apps remove the checks for
                // FROM_EXTERNAL_APP/FROM_NEW.
                if (!mVisibleTab.isClosing()
                        && (!isFromExternalApp || type != TabSelectionType.FROM_NEW)) {
                    cacheTabBitmap(mVisibleTab);
                }
                mVisibleTab.hide(TabHidingType.CHANGED_TABS);
                mTabSaver.addTabToSaveQueue(mVisibleTab);
            }
            mVisibleTab = null;
        }

        if (tab == null) {
            notifyChanged();
            return;
        }

        // We hit this case when the user enters tab switcher and comes back to the current tab
        // without actual tab switch.
        if (mVisibleTab == tab && !mVisibleTab.isHidden()) {
            // The current tab might have been killed by the os while in tab switcher.
            tab.loadIfNeeded();
            // |tabToDropImportance| must be null, so no need to drop importance.
            return;
        }
        mVisibleTab = tab;

        // Don't execute the tab display part if Chrome has just been sent to background. This
        // avoids unecessary work (tab restore) and prevents pollution of tab display metrics - see
        // http://crbug.com/316166.
        if (type != TabSelectionType.FROM_EXIT) {
            tab.show(type);
            tab.getId();
            tab.isBeingRestored();
        }
    }

    private void cacheTabBitmap(Tab tabToCache) {
        // Trigger a capture of this tab.
        if (tabToCache == null) return;
        mTabContentManager.cacheTabThumbnail(tabToCache);
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return mSessionRestoreInProgress.get();
    }

    // TODO(tedchoc): Remove the need for this to be exposed.
    @Override
    public void notifyChanged() {
        super.notifyChanged();
    }

    @VisibleForTesting
    public TabPersistentStore getTabPersistentStoreForTesting() {
        return mTabSaver;
    }
}
