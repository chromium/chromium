// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for creating {@link TabGroupModelFilter}s to be applied on the {@link
 * TabModel}s. It always owns two {@link TabGroupModelFilter}s, one for normal {@link TabModel} and
 * one for incognito {@link TabModel}.
 */
@NullMarked
public class TabGroupModelFilterProvider {
    private final List<TabModelObserver> mPendingTabModelObserver = new ArrayList<>();
    private final ObservableSupplierImpl<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier = new ObservableSupplierImpl<>();
    private final Callback<TabModel> mCurrentTabModelObserver = this::onCurrentTabModelChanged;

    private List<TabGroupModelFilterInternal> mTabGroupModelFilterInternalList =
            Collections.emptyList();
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable CallbackController mCallbackController = new CallbackController();

    /*package*/ TabGroupModelFilterProvider() {}

    /*package*/ void init(
            TabGroupModelFilterFactory tabGroupModelFilterFactory,
            TabUngrouperFactory tabUngrouperFactory,
            TabModelSelector tabModelSelector,
            List<TabModelInternal> tabModels) {
        assert mTabGroupModelFilterInternalList.isEmpty();
        assert tabModels.size() > 0;

        mTabModelSelector = tabModelSelector;

        List<TabGroupModelFilterInternal> filters = new ArrayList<>(tabModels.size());
        for (TabModelInternal tabModel : tabModels) {
            boolean isIncognitoBranded = tabModel.isIncognitoBranded();
            TabUngrouper tabUngrouper =
                    tabUngrouperFactory.create(
                            isIncognitoBranded, () -> getTabGroupModelFilter(isIncognitoBranded));
            filters.add(
                    tabGroupModelFilterFactory.createTabGroupModelFilter(tabModel, tabUngrouper));
        }

        mTabGroupModelFilterInternalList = Collections.unmodifiableList(filters);
        // Registers the pending observers.
        for (TabModelObserver observer : mPendingTabModelObserver) {
            for (TabGroupModelFilter tabGroupModelFilter : mTabGroupModelFilterInternalList) {
                tabGroupModelFilter.addObserver(observer);
            }
        }
        mPendingTabModelObserver.clear();

        assumeNonNull(mCallbackController);
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (unusedTabModelSelector) -> {
                            markTabStateInitialized();
                        }));
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
    }

    /**
     * This method adds {@link TabModelObserver} to both {@link TabGroupModelFilter}s. Caches the
     * observer until {@link TabGroupModelFilter}s are created.
     *
     * @param observer {@link TabModelObserver} to add.
     */
    public void addTabGroupModelFilterObserver(TabModelObserver observer) {
        if (mTabGroupModelFilterInternalList.isEmpty()) {
            mPendingTabModelObserver.add(observer);
            return;
        }

        for (TabGroupModelFilter filter : mTabGroupModelFilterInternalList) {
            filter.addObserver(observer);
        }
    }

    /**
     * This method removes {@link TabModelObserver} from both {@link TabGroupModelFilter}s.
     *
     * @param observer {@link TabModelObserver} to remove.
     */
    public void removeTabGroupModelFilterObserver(TabModelObserver observer) {
        if (mTabGroupModelFilterInternalList.isEmpty() && !mPendingTabModelObserver.isEmpty()) {
            mPendingTabModelObserver.remove(observer);
            return;
        }

        for (TabGroupModelFilter filter : mTabGroupModelFilterInternalList) {
            filter.removeObserver(observer);
        }
    }

    /**
     * This method returns a specific {@link TabGroupModelFilter}.
     *
     * @param isIncognito Use to indicate which {@link TabGroupModelFilter} to return.
     * @return A {@link TabGroupModelFilter}. This returns null, if this called before native
     *     library is initialized.
     */
    public @Nullable TabGroupModelFilter getTabGroupModelFilter(boolean isIncognito) {
        for (TabGroupModelFilter filter : mTabGroupModelFilterInternalList) {
            if (filter.getTabModel().isIncognito() == isIncognito) {
                return filter;
            }
        }
        return null;
    }

    /**
     * This method returns the current {@link TabGroupModelFilter}.
     *
     * @return The current {@link TabGroupModelFilter}. This returns null, if this called before
     *     native library is initialized.
     */
    public @Nullable TabGroupModelFilter getCurrentTabGroupModelFilter() {
        return mCurrentTabGroupModelFilterSupplier.get();
    }

    /** Returns an observable supplier for the current tab model filter. */
    public ObservableSupplier<@Nullable TabGroupModelFilter>
            getCurrentTabGroupModelFilterSupplier() {
        return mCurrentTabGroupModelFilterSupplier;
    }

    /** This method destroys all owned {@link TabGroupModelFilter}. */
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        for (TabGroupModelFilterInternal filter : mTabGroupModelFilterInternalList) {
            filter.destroy();
        }
        mPendingTabModelObserver.clear();
        cleanupTabModelSelectorObservers();
    }

    private void markTabStateInitialized() {
        for (TabGroupModelFilterInternal filter : mTabGroupModelFilterInternalList) {
            filter.markTabStateInitialized();
        }
    }

    private void onCurrentTabModelChanged(TabModel model) {
        for (TabGroupModelFilter filter : mTabGroupModelFilterInternalList) {
            if (filter.getTabModel().isActiveModel()) {
                mCurrentTabGroupModelFilterSupplier.set(filter);
                return;
            }
        }
        assert model == null
                : "Non-null current TabModel should set an active TabGroupModelFilter.";
        mCurrentTabGroupModelFilterSupplier.set(null);
    }

    private void cleanupTabModelSelectorObservers() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
    }

    /** Reset the internal filter list to allow initialization again. */
    public void resetTabGroupModelFilterListForTesting() {
        mTabGroupModelFilterInternalList = Collections.emptyList();
        mCurrentTabGroupModelFilterSupplier.set(null);
        cleanupTabModelSelectorObservers();
        mCallbackController = new CallbackController();
    }
}
