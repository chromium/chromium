// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for creating {@link TabModelFilter}s to be applied on the {@link
 * TabModel}s. It always owns two {@link TabModelFilter}s, one for normal {@link TabModel} and one
 * for incognito {@link TabModel}.
 */
public class TabModelFilterProvider {
    private final List<TabModelObserver> mPendingTabModelObserver = new ArrayList<>();
    private final ObservableSupplierImpl<TabModelFilter> mCurrentTabModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<TabModel> mCurrentTabModelObserver = this::onCurrentTabModelChanged;

    private List<TabModelFilterBase> mTabModelFilterBaseList = Collections.emptyList();
    private TabModelSelector mTabModelSelector;
    private CallbackController mCallbackController = new CallbackController();

    /*package*/ TabModelFilterProvider() {}

    /*package*/ void init(
            @NonNull TabModelFilterFactory tabModelFilterFactory,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull List<TabModel> tabModels) {
        assert mTabModelFilterBaseList.isEmpty();
        assert tabModels.size() > 0;

        mTabModelSelector = tabModelSelector;

        List<TabModelFilterBase> filters = new ArrayList<>();
        for (TabModel tabModel : tabModels) {
            filters.add(tabModelFilterFactory.createTabModelFilter(tabModel));
        }

        mTabModelFilterBaseList = Collections.unmodifiableList(filters);
        // Registers the pending observers.
        for (TabModelObserver observer : mPendingTabModelObserver) {
            for (TabModelFilter tabModelFilter : mTabModelFilterBaseList) {
                tabModelFilter.addObserver(observer);
            }
        }
        mPendingTabModelObserver.clear();

        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (unusedTabModelSelector) -> {
                            markTabStateInitialized();
                        }));
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
    }

    /**
     * This method adds {@link TabModelObserver} to both {@link TabModelFilter}s. Caches the
     * observer until {@link TabModelFilter}s are created.
     *
     * @param observer {@link TabModelObserver} to add.
     */
    public void addTabModelFilterObserver(TabModelObserver observer) {
        if (mTabModelFilterBaseList.isEmpty()) {
            mPendingTabModelObserver.add(observer);
            return;
        }

        for (TabModelFilter filter : mTabModelFilterBaseList) {
            filter.addObserver(observer);
        }
    }

    /**
     * This method removes {@link TabModelObserver} from both {@link TabModelFilter}s.
     *
     * @param observer {@link TabModelObserver} to remove.
     */
    public void removeTabModelFilterObserver(TabModelObserver observer) {
        if (mTabModelFilterBaseList.isEmpty() && !mPendingTabModelObserver.isEmpty()) {
            mPendingTabModelObserver.remove(observer);
            return;
        }

        for (TabModelFilter filter : mTabModelFilterBaseList) {
            filter.removeObserver(observer);
        }
    }

    /**
     * This method returns a specific {@link TabModelFilter}.
     *
     * @param isIncognito Use to indicate which {@link TabModelFilter} to return.
     * @return A {@link TabModelFilter}. This returns null, if this called before native library is
     *     initialized.
     */
    public TabModelFilter getTabModelFilter(boolean isIncognito) {
        for (TabModelFilter filter : mTabModelFilterBaseList) {
            if (filter.isIncognito() == isIncognito) {
                return filter;
            }
        }
        return null;
    }

    /**
     * This method returns the current {@link TabModelFilter}.
     * @return The current {@link TabModelFilter}. This returns null, if this called before native
     * library is initialized.
     */
    public TabModelFilter getCurrentTabModelFilter() {
        return mCurrentTabModelFilterSupplier.get();
    }

    /** Returns an observable supplier for the current tab model filter. */
    public ObservableSupplier<TabModelFilter> getCurrentTabModelFilterSupplier() {
        return mCurrentTabModelFilterSupplier;
    }

    /** This method destroys all owned {@link TabModelFilter}. */
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        for (TabModelFilter filter : mTabModelFilterBaseList) {
            filter.destroy();
        }
        mPendingTabModelObserver.clear();
        cleanupTabModelSelectorObservers();
    }

    private void markTabStateInitialized() {
        for (TabModelFilterBase filter : mTabModelFilterBaseList) {
            filter.markTabStateInitialized();
        }
    }

    private void onCurrentTabModelChanged(TabModel model) {
        for (TabModelFilter filter : mTabModelFilterBaseList) {
            if (filter.isCurrentlySelectedFilter()) {
                mCurrentTabModelFilterSupplier.set(filter);
                return;
            }
        }
        assert model == null : "Non-null current TabModel should set an active TabModelFilter.";
        mCurrentTabModelFilterSupplier.set(null);
    }

    private void cleanupTabModelSelectorObservers() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
    }

    /** Reset the internal filter list to allow initialization again. */
    public void resetTabModelFilterListForTesting() {
        mTabModelFilterBaseList = Collections.emptyList();
        mCurrentTabModelFilterSupplier.set(null);
        cleanupTabModelSelectorObservers();
        mCallbackController = new CallbackController();
    }
}
