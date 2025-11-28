// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.components.tabs.TabStripCollection;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Function;

/** Implement methods shared across the different model implementations. */
@NullMarked
public abstract class TabModelSelectorBase
        implements TabModelSelector, IncognitoTabModelObserver, TabModelDelegate {
    private static final int MODEL_NOT_FOUND = -1;

    private static @Nullable TabModelSelectorObserver sObserverForTesting;

    /**
     * Elements in {@link mTabModels} should be kept in sync with elements in {@link
     * mTabModelInternals}. These could be the same list; however, the casting of {@code
     * (List<TabModel>)(List<? extends TabModel>) mTabModelInternals} is potentially dangerous and
     * could crash at runtime if the list were mutated or cast with the wrong type.
     */
    private final List<TabModel> mTabModels = new ArrayList<>();

    private final List<TabModelInternal> mTabModelInternals = new ArrayList<>();
    private @Nullable IncognitoTabModel mIncognitoTabModel;

    private final TabGroupModelFilterProvider mTabGroupModelFilterProvider =
            new TabGroupModelFilterProvider();
    private final ObservableSupplierImpl<TabModel> mTabModelSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;
    private final ObservableSupplier<Integer> mCurrentModelTabCountSupplier;

    private final ObserverList<TabModelSelectorObserver> mObservers = new ObserverList<>();
    private final ObserverList<IncognitoTabModelObserver> mIncognitoObservers =
            new ObserverList<>();

    private final TabCreatorManager mTabCreatorManager;

    private final Callback<TabModel> mIncognitoReauthDialogDelegateCallback;
    protected @Nullable IncognitoReauthDialogDelegate mIncognitoReauthDialogDelegate;

    private boolean mTabStateInitialized;
    private boolean mStartIncognito;
    private boolean mReparentingInProgress;

    protected TabModelSelectorBase(TabCreatorManager tabCreatorManager, boolean startIncognito) {
        mTabCreatorManager = tabCreatorManager;
        mStartIncognito = startIncognito;
        // Notify the re-auth code first so we show the re-auth dialog first.
        mIncognitoReauthDialogDelegateCallback =
                (tabModel) -> {
                    if (mIncognitoReauthDialogDelegate != null && tabModel.isIncognito()) {
                        mIncognitoReauthDialogDelegate.onBeforeIncognitoTabModelSelected();
                    }
                };
        mTabModelSupplier.addObserver(mIncognitoReauthDialogDelegateCallback);
        mCurrentTabSupplier =
                mTabModelSupplier.createTransitive(
                        (Function<TabModel, ObservableSupplier<@Nullable Tab>>)
                                TabModel::getCurrentTabSupplier);
        mCurrentModelTabCountSupplier =
                mTabModelSupplier.createTransitive(TabModel::getTabCountSupplier);
    }

    // Do not use @Initializer. Not called immediately after constructor.
    protected final void initialize(
            TabModelHolder normalModelHolder, IncognitoTabModelHolder incognitoModelHolder) {
        // Only normal and incognito supported for now.
        assert mTabModelInternals.isEmpty();

        mTabModelInternals.add(normalModelHolder.tabModel);
        mTabModelInternals.add(incognitoModelHolder.tabModel);
        mTabModels.addAll(mTabModelInternals);
        mIncognitoTabModel = incognitoModelHolder.tabModel;
        int activeModelIndex = getModelIndex(mStartIncognito);
        assert activeModelIndex != MODEL_NOT_FOUND;
        mTabGroupModelFilterProvider.init(
                this,
                List.of(
                        normalModelHolder.tabGroupModelFilter,
                        incognitoModelHolder.tabGroupModelFilter));

        TabModelObserver tabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        notifyChanged();
                        notifyNewTabCreated(tab, creationState);
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        notifyChanged();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        notifyChanged();
                    }
                };

        mTabGroupModelFilterProvider.addTabGroupModelFilterObserver(tabModelObserver);

        if (sObserverForTesting != null) {
            addObserver(sObserverForTesting);
        }
        mIncognitoTabModel.addIncognitoObserver(this);

        incognitoModelHolder.tabModel.setActive(mStartIncognito);
        normalModelHolder.tabModel.setActive(!mStartIncognito);
        mTabModelSupplier.set(mTabModelInternals.get(activeModelIndex));

        notifyChanged();
    }

    public static void setObserverForTests(@Nullable TabModelSelectorObserver observer) {
        sObserverForTesting = observer;
        ResettersForTesting.register(() -> sObserverForTesting = null);
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this class
     * can be initialized.
     *
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    public void onNativeLibraryReady(
            TabContentManager tabContentProvider, boolean wasTabCollectionsActive) {}

    @Override
    public void onTabsViewShown() {}

    @Override
    public void selectModel(boolean incognito) {
        if (mTabModelInternals.size() == 0) {
            mStartIncognito = incognito;
            return;
        }
        int newIndex = getModelIndex(incognito);
        assert newIndex != MODEL_NOT_FOUND;
        if (mTabModelInternals.get(newIndex) == mTabModelSupplier.get()) return;

        TabModelInternal newModel = mTabModelInternals.get(newIndex);
        TabModelInternal previousModel = (TabModelInternal) assumeNonNull(mTabModelSupplier.get());
        previousModel.setActive(false);
        newModel.setActive(true);
        mTabModelSupplier.set(newModel);
    }

    @Override
    public @Nullable Tab getCurrentTab() {
        // TODO(crbug.com/40287823): Migrate this to use mCurrentTabSupplier.get(). Presently, a
        // large number of tests depend on using this from a non-UI thread.
        return TabModelUtils.getCurrentTab(getCurrentModel());
    }

    @Override
    public int getCurrentTabId() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    @Override
    public @Nullable TabModel getModelForTabId(int id) {
        for (int i = 0; i < mTabModelInternals.size(); i++) {
            TabModel model = mTabModelInternals.get(i);
            if (model.getTabById(id) != null || model.isClosurePending(id)) {
                return model;
            }
        }
        return null;
    }

    @Override
    public TabModel getCurrentModel() {
        if (mTabModelInternals.size() == 0) return EmptyTabModel.getInstance(false);
        return assumeNonNull(mTabModelSupplier.get());
    }

    @Override
    public ObservableSupplier<TabModel> getCurrentTabModelSupplier() {
        return mTabModelSupplier;
    }

    @Override
    public ObservableSupplier<@Nullable Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public ObservableSupplier<Integer> getCurrentModelTabCountSupplier() {
        return mCurrentModelTabCountSupplier;
    }

    @Override
    public TabModel getModel(boolean incognito) {
        int index = getModelIndex(incognito);
        if (index == MODEL_NOT_FOUND) return EmptyTabModel.getInstance(false);
        return mTabModelInternals.get(index);
    }

    @Override
    public TabGroupModelFilter getFilter(boolean incognito) {
        return assumeNonNull(mTabGroupModelFilterProvider.getTabGroupModelFilter(incognito));
    }

    private int getModelIndex(boolean incognito) {
        for (int i = 0; i < mTabModelInternals.size(); i++) {
            if (incognito == mTabModelInternals.get(i).isIncognito()) return i;
        }
        return MODEL_NOT_FOUND;
    }

    @Override
    public TabGroupModelFilterProvider getTabGroupModelFilterProvider() {
        return mTabGroupModelFilterProvider;
    }

    @Override
    public boolean isIncognitoSelected() {
        if (mTabModelInternals.size() == 0) return mStartIncognito;
        return getCurrentModel().isIncognito();
    }

    @Override
    public boolean isIncognitoBrandedModelSelected() {
        if (mTabModelInternals.size() == 0) return mStartIncognito;
        return getCurrentModel().isIncognitoBranded();
    }

    @Override
    public boolean isOffTheRecordModelSelected() {
        if (mTabModelInternals.size() == 0) return mStartIncognito;
        return getCurrentModel().isOffTheRecord();
    }

    @Override
    public List<TabModel> getModels() {
        return mTabModels;
    }

    @Override
    public TabCreatorManager getTabCreatorManager() {
        return mTabCreatorManager;
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams,
            @TabLaunchType int type,
            @Nullable Tab parent,
            boolean incognito) {
        return assumeNonNull(mTabCreatorManager
                .getTabCreator(incognito)
                .createNewTab(loadUrlParams, type, parent));
    }

    @Override
    public boolean tryCloseTab(TabClosureParams tabClosureParams, boolean allowDialog) {
        if (tabClosureParams.tabs == null
                || tabClosureParams.tabs.size() != 1
                || tabClosureParams.tabCloseType != TabCloseType.SINGLE) {
            assert false : "Invalid tab closure params received for tryCloseTab.";
            return false;
        }
        Tab tab = tabClosureParams.tabs.get(0);
        boolean isClosing = tab.isClosing() && !tab.isDestroyed();
        for (int i = 0; i < getModels().size(); i++) {
            TabModel model = mTabModelInternals.get(i);
            if (isClosing) {
                // If the tab is closing and not destroyed it should be in the comprehensive model
                // of one of the tab models. Find its model and commit the tab closure.
                TabList comprehensiveModel = model.getComprehensiveModel();
                if (comprehensiveModel.indexOf(tab) > TabList.INVALID_TAB_INDEX) {
                    model.commitTabClosure(tab.getId());
                    return true;
                }
            } else if (model.indexOf(tab) > TabList.INVALID_TAB_INDEX) {
                model.getTabRemover().closeTabs(tabClosureParams, allowDialog);
                return true;
            }
        }

        // In case the tab needs to be closed while a reparenting task is executing. This could be
        // the case for navigations progressing while the tab is being moved between web clients.
        if (tab.isDetached()) {
            tab.setDidCloseWhileDetached();
            return true;
        }

        if (getModels().isEmpty()) {
            // Tab may be destroyed here via Tab#destroy(). It is skipped for now
            // to examine its potential side effect on crbug.com/325558929.
            return true;
        } else {
            assert false
                    : "Tried to close a tab that is not in any model!"
                            + " Activity class name "
                            + getTabActivityName(tab)
                            + " Is closing "
                            + tab.isClosing()
                            + " Is destroyed "
                            + tab.isDestroyed()
                            + " Is detached "
                            + tab.isDetached();
            return false;
        }
    }

    private String getTabActivityName(Tab tab) {
        if (tab.getWindowAndroid() == null) return "unknown";
        Activity activity =
                ContextUtils.activityFromContext(tab.getWindowAndroid().getContext().get());
        return activity == null ? "unknown" : activity.getClass().getName();
    }

    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < mTabModelInternals.size(); i++) {
            mTabModelInternals.get(i).commitAllTabClosures();
        }
    }

    @Override
    public @Nullable Tab getTabById(int id) {
        for (int i = 0; i < getModels().size(); i++) {
            Tab tab = mTabModelInternals.get(i).getTabById(id);
            if (tab != null) return tab;
        }
        return null;
    }

    @Override
    public int getTotalTabCount() {
        int count = 0;
        for (int i = 0; i < getModels().size(); i++) {
            count += mTabModelInternals.get(i).getCount();
        }
        return count;
    }

    @Override
    public int getTotalPinnedTabCount() {
        int count = 0;
        for (int i = 0; i < getModels().size(); i++) {
            count += mTabModelInternals.get(i).getPinnedTabsCount();
        }
        return count;
    }

    @Override
    public void addObserver(TabModelSelectorObserver observer) {
        if (!mObservers.hasObserver(observer)) mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelSelectorObserver observer) {
        mObservers.removeObserver(observer);
    }

    /** Marks the task state being initialized and notifies observers. */
    public void markTabStateInitialized() {
        if (mTabStateInitialized) return;
        mTabStateInitialized = true;
        for (TabModelSelectorObserver listener : mObservers) listener.onTabStateInitialized();
    }

    @Override
    public boolean isTabStateInitialized() {
        return mTabStateInitialized;
    }

    @Override
    public void destroy() {
        for (TabModelSelectorObserver listener : mObservers) listener.onDestroyed();
        mTabModelSupplier.removeObserver(mIncognitoReauthDialogDelegateCallback);
        mTabGroupModelFilterProvider.destroy();

        if (mIncognitoTabModel != null) {
            mIncognitoTabModel.removeIncognitoObserver(this);
        }
        for (int i = 0; i < getModels().size(); i++) mTabModelInternals.get(i).destroy();
        mTabModelInternals.clear();
        mTabModels.clear();
    }

    /**
     * Notifies all the listeners that the {@link TabModelSelector} or its {@link TabModel} has
     * changed.
     */
    // TODO(tedchoc): Remove the need for this to be exposed.
    public void notifyChanged() {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onChange();
        }
    }

    /**
     * Notifies all the listeners that a new tab has been created.
     * @param tab The tab that has been created.
     * @param creationState How the tab was created.
     */
    private void notifyNewTabCreated(Tab tab, @TabCreationState int creationState) {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onNewTabCreated(tab, creationState);
        }
    }

    /**
     * Notifies all the listeners that a tab has been hidden to switch to another.
     *
     * @param tab The tab that has been hidden.
     */
    protected void notifyTabHidden(Tab tab) {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onTabHidden(tab);
        }
    }

    @Override
    public void enterReparentingMode() {
        mReparentingInProgress = true;
    }

    @Override
    public boolean isReparentingInProgress() {
        return mReparentingInProgress;
    }

    @Override
    public void addIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver) {
        mIncognitoObservers.addObserver(incognitoObserver);
    }

    @Override
    public void removeIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver) {
        mIncognitoObservers.removeObserver(incognitoObserver);
    }

    @Override
    public void wasFirstTabCreated() {
        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.wasFirstTabCreated();
        }
    }

    @Override
    public void didBecomeEmpty() {
        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.didBecomeEmpty();
        }
    }

    @Override
    public void setIncognitoReauthDialogDelegate(
            IncognitoReauthDialogDelegate incognitoReauthDialogDelegate) {
        mIncognitoReauthDialogDelegate = incognitoReauthDialogDelegate;
    }

    @Override
    public @Nullable TabModel getTabModelForTabStripCollection(
            TabStripCollection tabStripCollection) {
        for (TabModel tabModel : getModels()) {
            TabStripCollection modelCollection = tabModel.getTabStripCollection();
            if (!Objects.equals(modelCollection, tabStripCollection)) continue;
            return tabModel;
        }
        return null;
    }
}
