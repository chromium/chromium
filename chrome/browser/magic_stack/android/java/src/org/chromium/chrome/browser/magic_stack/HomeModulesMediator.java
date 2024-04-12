// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** The mediator which implements the logic to add, update and remove modules. */
public class HomeModulesMediator {
    private static final int INVALID_INDEX = -1;

    /** Time to wait before rejecting any module response in milliseconds. */
    public static final long MODULE_FETCHING_TIMEOUT_MS = 5000L;

    private final ModelList mModel;
    private final ModuleRegistry mModuleRegistry;

    /** A map of <ModuleType, ModuleProvider>. */
    private final Map<Integer, ModuleProvider> mModuleTypeToModuleProviderMap = new HashMap<>();

    /** A map of <ModuleType, the ranking of this module from segmentation service>. */
    private final Map<Integer, Integer> mModuleTypeToRankingIndexMap = new HashMap<>();

    private final Handler mHandler = new Handler();

    /**
     * An array of cached responses (data) from modules. The size of the array is the number of
     * modules to show.
     */
    @Nullable private SimpleRecyclerViewAdapter.ListItem[] mModuleFetchResultsCache;

    /**
     * An array of cached responses from modules to indicate whether they have data to show. There
     * are three valid states: 1) null: module doesn't respond yet; 2) true: module has data to
     * show; 3) false: module doesn't have data to show. The size of the array is the number of
     * modules to show.
     */
    @Nullable private Boolean[] mModuleFetchResultsIndicator;

    /** The ranking index of the module whose response that the magic stack is waiting for. */
    private int mModuleResultsWaitingIndex;

    /** Whether a fetch of modules is in progress. */
    private boolean mIsFetchingModules;

    private boolean mIsShown;
    private Callback<Boolean> mSetVisibilityCallback;
    private long[] mShowModuleStartTimeMs;
    private List<Integer> mModuleListToShow;
    private @HostSurface int mHostSurface;

    /**
     * @param model The instance of {@link ModelList} of the RecyclerView.
     */
    public HomeModulesMediator(@NonNull ModelList model, @NonNull ModuleRegistry moduleRegistry) {
        mModel = model;
        mModuleRegistry = moduleRegistry;
    }

    /**
     * Builds and shows modules from the given module list.
     *
     * @param moduleList The list of sorted modules to show.
     * @param moduleDelegate The instance of the magic stack {@link ModuleDelegate}.
     */
    void buildModulesAndShow(
            @NonNull @ModuleType List<Integer> moduleList,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<Boolean> setVisibilityCallback) {
        if (mIsShown) {
            updateModules();
            return;
        }

        mSetVisibilityCallback = setVisibilityCallback;
        assert mModel.size() == 0;
        mIsFetchingModules = true;
        mIsShown = true;
        mHostSurface = moduleDelegate.getHostSurfaceType();
        mModuleListToShow = moduleList;
        cacheRanking(mModuleListToShow);

        mModuleResultsWaitingIndex = 0;
        mModuleFetchResultsCache = new SimpleRecyclerViewAdapter.ListItem[mModuleListToShow.size()];
        mModuleFetchResultsIndicator = new Boolean[mModuleListToShow.size()];
        mShowModuleStartTimeMs = new long[mModuleListToShow.size()];
        boolean hasModuleBuilt = false;

        for (int i = 0; i < mModuleListToShow.size(); i++) {
            int moduleType = mModuleListToShow.get(i);
            mShowModuleStartTimeMs[i] = SystemClock.elapsedRealtime();
            if (!mModuleRegistry.build(
                    moduleType,
                    moduleDelegate,
                    (moduleProvider) -> {
                        onModuleBuilt(moduleType, moduleProvider);
                    })) {
                // If the module hasn't registered a builder object to the ModuleRegister, the
                // #build() will return false immediately. Update the mModuleFetchResultsIndicator[]
                // to indicate that it isn't possible to show that module.
                mModuleFetchResultsIndicator[i] = false;
                // If we are currently waiting for the response of this module, increases the
                // mModuleResultsWaitingIndex. If the next module(s) based on the ranking have
                // responded before and cached, add them to the RecyclerView now.
                if (mModuleResultsWaitingIndex == i) {
                    mModuleResultsWaitingIndex++;
                    maybeMoveEarlyReceivedModulesToRecyclerView();
                }
            } else {
                hasModuleBuilt = true;
            }
        }
        // Don't start the timer if the magic stack isn't waiting for any module to be load.
        if (hasModuleBuilt) {
            mHandler.postDelayed(this::onModuleFetchTimeOut, MODULE_FETCHING_TIMEOUT_MS);
        } else {
            mIsFetchingModules = false;
        }
    }

    /**
     * Caches the ranking of the modules.
     *
     * @param moduleList The list of modules sorted by ranking.
     */
    @VisibleForTesting
    void cacheRanking(@NonNull @ModuleType List<Integer> moduleList) {
        for (int i = 0; i < moduleList.size(); i++) {
            mModuleTypeToRankingIndexMap.put(moduleList.get(i), i);
        }
    }

    /**
     * Called when a module is built.
     *
     * @param moduleType The type of the module.
     * @param moduleProvider The newly created instance of the module.
     */
    @VisibleForTesting
    void onModuleBuilt(int moduleType, @NonNull ModuleProvider moduleProvider) {
        mModuleTypeToModuleProviderMap.put(moduleType, moduleProvider);
        moduleProvider.showModule();
    }

    /**
     * Adds the module's PropertyModel to the RecyclerView or caches it if waiting for the responses
     * from modules with higher ranking. Changes the visibility of the RecyclerView if the first
     * available module with the highest ranking is ready.
     *
     * @param moduleType The type of the module.
     * @param propertyModel The object of {@link PropertyModel} of the module. Null if the module
     *     doesn't have any data to show.
     */
    @VisibleForTesting
    void addToRecyclerViewOrCache(
            @ModuleType int moduleType, @Nullable PropertyModel propertyModel) {
        if (!mModuleTypeToRankingIndexMap.containsKey(moduleType)) {
            // TODO(b/326081541): add an assert here to prevent a module add itself to the magic
            // stack after sending a onDataFetchFailed() response.
            return;
        }

        int index = mModuleTypeToRankingIndexMap.get(moduleType);
        long duration = SystemClock.elapsedRealtime() - mShowModuleStartTimeMs[index];
        if (!mIsFetchingModules) {
            HomeModulesMetricsUtils.recordFetchDataTimeOutDuration(
                    mHostSurface, moduleType, duration);
            return;
        }

        // When the magic stack receives a onDataFetchFailed() response, it calls
        // ModuleProvider#hideModule() to allow the module to clean up.
        boolean isHideModuleCalled = false;
        // If this module has responded before, update its data on the RecyclerView.
        if (index < mModuleResultsWaitingIndex) {
            if (propertyModel != null) {
                updateRecyclerView(moduleType, index, propertyModel);
            } else {
                remove(moduleType, index);
                // In remove(), ModuleProvider#hideModule() has been called.
                isHideModuleCalled = true;
            }
        } else if (index == mModuleResultsWaitingIndex) {
            if (propertyModel != null) {
                // This module is the highest ranking one that we are waiting for, adds its data to
                // the RecyclerView.
                append(new SimpleRecyclerViewAdapter.ListItem(moduleType, propertyModel));
            }
            // Stores the responses based on whether the module has data or not and increases the
            // waiting index for the next highest ranking module.
            mModuleFetchResultsIndicator[index] = propertyModel != null;
            mModuleResultsWaitingIndex++;
            // It is possible that there are modules with lower ranking have responded and cached,
            // move their data to the RecyclerView now.
            maybeMoveEarlyReceivedModulesToRecyclerView();
        } else {
            // If we are waiting for responses from higher ranking modules, cache the response.
            mModuleFetchResultsIndicator[index] = propertyModel != null;
            mModuleFetchResultsCache[index] =
                    propertyModel != null
                            ? new SimpleRecyclerViewAdapter.ListItem(moduleType, propertyModel)
                            : null;
        }

        if (propertyModel == null) {
            if (!isHideModuleCalled) {
                // When a module has no data to show, call ModuleProvider#hideModule() to allow the
                // module to clean up.
                hideModuleOnDataFetchFailed(moduleType);
            }
            HomeModulesMetricsUtils.recordFetchDataFailedDuration(
                    mHostSurface, moduleType, duration);
        } else {
            HomeModulesMetricsUtils.recordFetchDataDuration(mHostSurface, moduleType, duration);
        }
    }

    /** Updates the data of an existing module on the RecyclerView. */
    private void updateRecyclerView(
            @ModuleType int moduleType, int index, @NonNull PropertyModel propertyModel) {
        int position = findModuleIndexInRecyclerView(moduleType, index);
        if (position == INVALID_INDEX) return;

        mModel.update(position, new SimpleRecyclerViewAdapter.ListItem(moduleType, propertyModel));
    }

    /**
     * Returns the module's index on the RecyclerView. This index is different from the ranking of
     * the module, since modules without data aren't added to the RecyclerView.
     */
    @VisibleForTesting
    int findModuleIndexInRecyclerView(@ModuleType int moduleType, int index) {
        int endIndex = Math.min(index, mModel.size() - 1);
        // The position of a module on the RecyclerView should be less or equal to its ranking
        // index, since some modules aren't shown due to lack of data. Therefore, it is faster to
        // find the position of a module starting from its ranking index.
        for (int i = endIndex; i >= 0; i--) {
            if (mModel.get(i).type == moduleType) {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    /** Adds the cached responses to the RecyclerView if exist. */
    private void maybeMoveEarlyReceivedModulesToRecyclerView() {
        while (mModuleResultsWaitingIndex < mModuleFetchResultsIndicator.length) {
            if (mModuleFetchResultsIndicator[mModuleResultsWaitingIndex] == null) return;

            if (mModuleFetchResultsIndicator[mModuleResultsWaitingIndex]) {
                append(mModuleFetchResultsCache[mModuleResultsWaitingIndex]);
            }
            mModuleResultsWaitingIndex++;
        }
    }

    /** Adds all of the cached responses to the RecyclerView after time out. */
    @VisibleForTesting
    void onModuleFetchTimeOut() {
        // It is possible that onModuleFetchTimeOut() is called after home modules hide, early exits
        // here.
        if (!mIsFetchingModules) return;

        // Will reject any late responses from modules.
        mIsFetchingModules = false;

        while (mModuleResultsWaitingIndex < mModuleFetchResultsIndicator.length) {
            var hasResult = mModuleFetchResultsIndicator[mModuleResultsWaitingIndex];
            if (hasResult == null) {
                // Case 1: no response received.
                @ModuleType int moduleType = mModuleListToShow.get(mModuleResultsWaitingIndex);
                HomeModulesMetricsUtils.recordFetchDataTimeOutType(mHostSurface, moduleType);
                hideModuleOnDataFetchFailed(moduleType);
            } else if (hasResult) {
                // Case 2: received a response with data to show.
                var cachedResponse = mModuleFetchResultsCache[mModuleResultsWaitingIndex];
                assert cachedResponse != null;
                // append() will change the visibility of the recyclerview if there isn't any module
                // added before time out.
                append(cachedResponse);
            }
            mModuleResultsWaitingIndex++;
        }
    }

    /**
     * Appends the item to the end of the RecyclerView. If it is the first module of the
     * RecyclerView, change the RecyclerView to be visible.
     *
     * @param item The item to add.
     */
    @VisibleForTesting
    void append(@NonNull SimpleRecyclerViewAdapter.ListItem item) {
        mModel.add(item);

        HomeModulesMetricsUtils.recordModuleBuiltPosition(
                mHostSurface, item.type, mModel.size() - 1);

        if (mModel.size() == 1) {
            mSetVisibilityCallback.onResult(true);

            // We use the build time of the first module as the starting time.
            long duration = SystemClock.elapsedRealtime() - mShowModuleStartTimeMs[0];
            HomeModulesMetricsUtils.recordFirstModuleShownDuration(mHostSurface, duration);
        }
    }

    // Called to hide the module when a module responds without any data to show.
    private void hideModuleOnDataFetchFailed(@ModuleType int moduleType) {
        ModuleProvider moduleProvider = mModuleTypeToModuleProviderMap.get(moduleType);
        moduleProvider.hideModule();
        mModuleTypeToModuleProviderMap.remove(moduleType);
    }

    /**
     * Removes the given module from the RecyclerView. Changes the RecyclerView to be invisible if
     * the last module is removed.
     *
     * @param moduleType The type of the module.
     */
    boolean remove(@ModuleType int moduleType) {
        if (!mIsShown || !mModuleTypeToModuleProviderMap.containsKey(moduleType)) {
            return false;
        }

        return remove(moduleType, mModuleTypeToRankingIndexMap.get(moduleType));
    }

    /**
     * Removes the given module from the RecyclerView. Changes the RecyclerView to be invisible if
     * the last module is removed.
     *
     * @param moduleType The type of the module.
     * @param index The original ranking index of the module.
     */
    private boolean remove(@ModuleType int moduleType, int index) {
        int position = findModuleIndexInRecyclerView(moduleType, index);
        if (position == INVALID_INDEX) return false;

        mModel.removeAt(position);
        ModuleProvider moduleProvider = mModuleTypeToModuleProviderMap.get(moduleType);
        moduleProvider.hideModule();
        mModuleTypeToModuleProviderMap.remove(moduleType);
        mModuleFetchResultsIndicator[index] = false;
        mModuleFetchResultsCache[index] = null;

        if (mModel.size() == 0) {
            hide();
        }
        return true;
    }

    /**
     * Called when the magic stack is hidden. Hides all of the modules and cleans up the magic
     * stack.
     */
    void hide() {
        if (!mIsShown) return;

        mIsFetchingModules = false;
        mIsShown = false;
        for (int i = 0; i < mModel.size(); i++) {
            int moduleType = mModel.get(i).type;
            ModuleProvider moduleProvider = mModuleTypeToModuleProviderMap.get(moduleType);
            moduleProvider.hideModule();
        }

        mModuleResultsWaitingIndex = 0;
        mModuleFetchResultsIndicator = null;
        mModuleFetchResultsCache = null;
        mShowModuleStartTimeMs = null;

        mModuleTypeToModuleProviderMap.clear();
        mModuleTypeToRankingIndexMap.clear();
        mModuleListToShow = null;

        mModel.clear();
        mSetVisibilityCallback.onResult(false);
    }

    /** Returns the instance of a module {@link ModuleProvider} of the given type. */
    ModuleProvider getModuleProvider(int moduleType) {
        return mModuleTypeToModuleProviderMap.get(moduleType);
    }

    /* Gets the rank of the module based on the given type. */
    int getModuleRank(@ModuleType int moduleType) {
        return findModuleIndexInRecyclerView(
                moduleType, mModuleTypeToRankingIndexMap.get(moduleType));
    }

    /**
     * Records whether the magic stack is scrollable and has been scrolled or not before it is
     * hidden or destroyed.
     */
    void recordMagicStackScroll(boolean hasHomeModulesBeenScrolled) {
        if (mModel.size() < 1) {
            return;
        }

        HomeModulesMetricsUtils.recordHomeModulesScrollState(
                mHostSurface, mModel.size() > 1, hasHomeModulesBeenScrolled);
    }

    /** Asks all of the modules being shown to reload their data if necessary. */
    void updateModules() {
        for (int i = 0; i < mModel.size(); i++) {
            @ModuleType int moduleType = mModel.get(i).type;
            ModuleProvider moduleProvider = mModuleTypeToModuleProviderMap.get(moduleType);
            assert moduleProvider != null;
            moduleProvider.updateModule();
        }
    }

    Map<Integer, ModuleProvider> getModuleTypeToModuleProviderMapForTesting() {
        return mModuleTypeToModuleProviderMap;
    }

    Map<Integer, Integer> getModuleTypeToRankingIndexMapForTesting() {
        return mModuleTypeToRankingIndexMap;
    }

    SimpleRecyclerViewAdapter.ListItem[] getModuleFetchResultsCacheForTesting() {
        return mModuleFetchResultsCache;
    }

    Boolean[] getModuleFetchResultsIndicatorForTesting() {
        return mModuleFetchResultsIndicator;
    }

    int getModuleResultsWaitingIndexForTesting() {
        return mModuleResultsWaitingIndex;
    }

    boolean getIsFetchingModulesForTesting() {
        return mIsFetchingModules;
    }
}
