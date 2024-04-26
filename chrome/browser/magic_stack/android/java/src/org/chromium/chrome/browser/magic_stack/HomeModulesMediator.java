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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** The mediator which implements the logic to add, update and remove modules. */
public class HomeModulesMediator {
    static final String USE_FRESHNESS_SCORE_PARAM = "use_freshness_score";
    private static final int INVALID_INDEX = -1;

    /** Time to wait before rejecting any module response in milliseconds. */
    public static final long MODULE_FETCHING_TIMEOUT_MS = 5000L;

    // Freshness score was logged older than 24h are considered stale, and rejected.
    static final long FRESHNESS_THRESHOLD_MS = TimeUnit.HOURS.toMillis(24);

    private final ModelList mModel;
    private final ModuleRegistry mModuleRegistry;
    private final ModuleDelegateHost mModuleDelegateHost;
    private final HomeModulesConfigManager mHomeModulesConfigManager;

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
    private SegmentationPlatformService mSegmentationPlatformService;
    private Set<Integer> mEnabledModuleSet;

    /**
     * @param model The instance of {@link ModelList} of the RecyclerView.
     */
    public HomeModulesMediator(
            @NonNull ModelList model,
            @NonNull ModuleRegistry moduleRegistry,
            @NonNull ModuleDelegateHost moduleDelegateHost,
            @NonNull HomeModulesConfigManager homeModulesConfigManager) {
        mModel = model;
        mModuleRegistry = moduleRegistry;
        mModuleDelegateHost = moduleDelegateHost;
        mHomeModulesConfigManager = homeModulesConfigManager;
    }

    /** Shows the magic stack with profile ready. */
    void showModules(
            Callback<Boolean> onHomeModulesShownCallback,
            ModuleDelegate moduleDelegate,
            SegmentationPlatformService segmentationPlatformService) {
        mSegmentationPlatformService = segmentationPlatformService;
        Set<Integer> filteredEnabledModuleSet = getFilteredEnabledModuleSet();

        if (mSegmentationPlatformService == null
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER)) {
            buildModulesAndShow(
                    getFixedModuleList(filteredEnabledModuleSet),
                    moduleDelegate,
                    onHomeModulesShownCallback,
                    /* durationMs= */ 0);
            return;
        }
        getSegmentationRanking(
                moduleDelegate, onHomeModulesShownCallback, filteredEnabledModuleSet);
    }

    private void buildModulesAndShow(
            List<Integer> moduleList,
            ModuleDelegate moduleDelegate,
            Callback<Boolean> onHomeModulesShownCallback,
            long durationMs) {
        // Record only if ranking is fetched from segmentation service.
        if (durationMs > 0) {
            HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(
                    mModuleDelegateHost.getHostSurfaceType(), durationMs);
        }
        if (moduleList == null) {
            onHomeModulesShownCallback.onResult(false);
            return;
        }

        moduleDelegate.prepareBuildAndShow();

        buildModulesAndShow(
                moduleList,
                moduleDelegate,
                (isVisible) -> {
                    onHomeModulesShownCallback.onResult(isVisible);
                });
    }

    /**
     * Builds and shows modules from the given module list.
     *
     * @param moduleList The list of sorted modules to show.
     * @param moduleDelegate The instance of the magic stack {@link ModuleDelegate}.
     */
    @VisibleForTesting
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
            // If there isn't any module to build, hide the magic stack now to clean up data.
            hide();
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
        if (position == INVALID_INDEX) {
            return;
        }

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
            if (mModuleFetchResultsIndicator[mModuleResultsWaitingIndex] == null) {
                return;
            }

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
        if (!mIsFetchingModules) {
            return;
        }

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

        if (mModel.size() == 0) {
            // It is possible that there isn't any module has data to show, hide the magic stack
            // now to clean up data.
            hide();
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
        if (position == INVALID_INDEX) {
            return false;
        }

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
        if (!mIsShown) {
            return;
        }

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

    /**
     * Updates the mEnabledModuleSet when the home modules' specific module type is disabled or
     * enabled.
     */
    void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled) {
        // The single tab module and the tab resumption modules are controlled by the same
        // preference key. Once it is turned on or off, both modules will be enabled or disabled.

        if (isEnabled) {
            // If the mEnabledModuleSet hasn't been initialized yet, skip here.
            if (mEnabledModuleSet != null) {
                if (moduleType == ModuleType.SINGLE_TAB
                        || moduleType == ModuleType.TAB_RESUMPTION) {
                    mEnabledModuleSet.add(ModuleType.SINGLE_TAB);
                    mEnabledModuleSet.add(ModuleType.TAB_RESUMPTION);
                } else {
                    mEnabledModuleSet.add(moduleType);
                }
            }
        } else {
            // If the mEnabledModuleSet hasn't been initialized yet, skip here.
            if (mEnabledModuleSet != null) {
                if (moduleType == ModuleType.SINGLE_TAB
                        || moduleType == ModuleType.TAB_RESUMPTION) {
                    mEnabledModuleSet.remove(ModuleType.SINGLE_TAB);
                    mEnabledModuleSet.remove(ModuleType.TAB_RESUMPTION);
                } else {
                    mEnabledModuleSet.remove(moduleType);
                }
            }
        }
    }

    /**
     * This method returns the list of enabled modules based on surface (Start/NTP). The list
     * returned is the intersection of modules that are enabled and available for the surface.
     */
    @VisibleForTesting
    List<Integer> getFixedModuleList(Set<Integer> filteredEnabledModuleSet) {
        List<Integer> generalModuleList = new ArrayList<>();
        if (filteredEnabledModuleSet.contains(ModuleType.PRICE_CHANGE)) {
            generalModuleList.add(ModuleType.PRICE_CHANGE);
        }

        if (filteredEnabledModuleSet.contains(ModuleType.SINGLE_TAB)) {
            generalModuleList.add(ModuleType.SINGLE_TAB);
        }

        for (@ModuleType Integer moduleType : filteredEnabledModuleSet) {
            if (moduleType == ModuleType.PRICE_CHANGE || moduleType == ModuleType.SINGLE_TAB) {
                continue;
            }

            generalModuleList.add(moduleType);
        }
        return generalModuleList;
    }

    /**
     * This function filters the mEnabledModuleSet by using heuristic logic.
     *
     * @return A set of the filtered enabled modules.
     */
    @VisibleForTesting
    Set<Integer> getFilteredEnabledModuleSet() {
        ensureEnabledModuleSetCreated();
        Set<Integer> set = new HashSet<>(mEnabledModuleSet);

        boolean combinedTabModules =
                combinedTabModules() && set.contains(ModuleType.TAB_RESUMPTION);
        boolean isHomeSurface = mModuleDelegateHost.isHomeSurface();
        boolean addAll = HomeModulesMetricsUtils.HOME_MODULES_SHOW_ALL_MODULES.getValue();

        if (combinedTabModules) {
            set.remove(ModuleType.SINGLE_TAB);
        } else if (isHomeSurface && !addAll) {
            set.remove(ModuleType.TAB_RESUMPTION);
        } else if (!isHomeSurface) {
            set.remove(ModuleType.SINGLE_TAB);
        }

        return set;
    }

    private void getSegmentationRanking(
            ModuleDelegate moduleDelegate,
            Callback<Boolean> onHomeModulesShownCallback,
            Set<Integer> filteredEnabledModuleSet) {
        // TODO(b/319530611): Convert the API to use on-demand option.
        PredictionOptions options = new PredictionOptions(false);
        long segmentationServiceCallTimeMs = SystemClock.elapsedRealtime();

        mSegmentationPlatformService.getClassificationResult(
                "android_home_module_ranker",
                options,
                /* inputContext= */ createInputContext(filteredEnabledModuleSet),
                result -> {
                    // It is possible that the result is received after the magic stack has been
                    // hidden, exit now.
                    long durationMs = SystemClock.elapsedRealtime() - segmentationServiceCallTimeMs;
                    if (mHomeModulesConfigManager == null) {
                        HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(
                                mModuleDelegateHost.getHostSurfaceType(), durationMs);
                        return;
                    }
                    buildModulesAndShow(
                            onGetClassificationResult(result, filteredEnabledModuleSet),
                            moduleDelegate,
                            onHomeModulesShownCallback,
                            durationMs);
                });
    }

    @VisibleForTesting
    InputContext createInputContext(Set<Integer> filteredEnabledModuleSet) {
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
                        USE_FRESHNESS_SCORE_PARAM,
                        false)
                || filteredEnabledModuleSet.isEmpty()) {
            return null;
        }

        InputContext inputContext = new InputContext();
        boolean isEntryAdded = false;
        for (Integer moduleType : filteredEnabledModuleSet) {
            long timeStamp = mHomeModulesConfigManager.getFreshnessScoreTimeStamp(moduleType);
            if (timeStamp == HomeModulesConfigManager.INVALID_TIMESTAMP
                    || SystemClock.elapsedRealtime() - timeStamp
                            >= HomeModulesMediator.FRESHNESS_THRESHOLD_MS) {
                continue;
            }

            int count = mHomeModulesConfigManager.getFreshnessCount(moduleType);
            if (count != HomeModulesConfigManager.INVALID_FRESHNESS_SCORE) {
                inputContext.addEntry(
                        HomeModulesMetricsUtils.getFreshnessInputContextString(moduleType),
                        ProcessedValue.fromInt(count));
                isEntryAdded = true;
            }
        }

        return isEntryAdded ? inputContext : null;
    }

    @VisibleForTesting
    List<Integer> onGetClassificationResult(
            ClassificationResult result, Set<Integer> filteredEnabledModuleSet) {
        List<Integer> moduleList;
        // If segmentation service fails, fallback to return fixed module list.
        if (result.status != PredictionStatus.SUCCEEDED || result.orderedLabels.isEmpty()) {
            moduleList = getFixedModuleList(filteredEnabledModuleSet);
        } else {
            moduleList = filterEnabledModuleList(result.orderedLabels, filteredEnabledModuleSet);
        }
        return moduleList;
    }

    /**
     * This method gets the list of enabled modules based on surface (Start/NTP) and returns the
     * list of modules which are present in both the previous list and the module list from the
     * model.
     */
    @VisibleForTesting
    List<Integer> filterEnabledModuleList(
            List<String> orderedModuleLabels, Set<Integer> filteredEnabledModuleSet) {
        List<Integer> moduleList = new ArrayList<>();

        for (String label : orderedModuleLabels) {
            @ModuleType
            int currentModuleType = HomeModulesMetricsUtils.convertLabelToModuleType(label);
            if (filteredEnabledModuleSet.contains(currentModuleType)) {
                moduleList.add(currentModuleType);
            }
        }
        return moduleList;
    }

    @VisibleForTesting
    void ensureEnabledModuleSetCreated() {
        if (mEnabledModuleSet != null) {
            return;
        }

        mEnabledModuleSet = mHomeModulesConfigManager.getEnabledModuleSet();
    }

    @VisibleForTesting
    boolean combinedTabModules() {
        return HomeModulesMetricsUtils.HOME_MODULES_COMBINE_TABS.getValue()
                && ChromeFeatureList.sTabResumptionModuleAndroid.isEnabled();
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

    boolean getIsShownForTesting() {
        return mIsShown;
    }
}
