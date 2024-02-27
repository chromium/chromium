// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.app.Activity;
import android.graphics.Point;
import android.os.SystemClock;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.SnapHelper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry.OnViewCreatedCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Root coordinator which is responsible for showing modules on home surfaces. */
public class HomeModulesCoordinator implements ModuleDelegate, OnViewCreatedCallback {
    private final ModuleDelegateHost mModuleDelegateHost;
    private HomeModulesMediator mMediator;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final HomeModulesRecyclerView mRecyclerView;
    private final ModelList mModel;
    private final HomeModulesContextMenuManager mHomeModulesContextMenuManager;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ModuleRegistry mModuleRegistry;

    private CirclePagerIndicatorDecoration mPageIndicatorDecoration;
    private SnapHelper mSnapHelper;
    private boolean mIsSnapHelperAttached;
    private int mItemPerScreen;
    private Set<Integer> mEnabledModuleSet;
    private HomeModulesConfigManager mHomeModulesConfigManager;
    private HomeModulesConfigManager.HomeModulesStateListener mHomeModulesStateListener;
    private SegmentationPlatformService mSegmentationPlatformService;

    /** It is non-null for tablets. */
    @Nullable private UiConfig mUiConfig;

    /** It is non-null for tablets. */
    @Nullable private DisplayStyleObserver mDisplayStyleObserver;

    @Nullable private Callback<Profile> mOnProfileAvailableObserver;
    private boolean mHasHomeModulesBeenScrolled;
    private RecyclerView.OnScrollListener mOnScrollListener;

    /**
     * @param activity The instance of {@link Activity}.
     * @param moduleDelegateHost The home surface which owns the magic stack.
     * @param parentView The parent view which holds the magic stack's RecyclerView.
     * @param homeModulesConfigManager The manager class which handles the enabling states of
     *     modules.
     * @param profileSupplier The supplier of the profile in use.
     * @param moduleRegistry The instance of {@link ModuleRegistry}.
     */
    public HomeModulesCoordinator(
            @NonNull Activity activity,
            @NonNull ModuleDelegateHost moduleDelegateHost,
            @NonNull ViewGroup parentView,
            @NonNull HomeModulesConfigManager homeModulesConfigManager,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ModuleRegistry moduleRegistry) {
        mModuleDelegateHost = moduleDelegateHost;
        mHomeModulesConfigManager = homeModulesConfigManager;
        mHomeModulesStateListener = this::onModuleConfigChanged;
        mHomeModulesConfigManager.addListener(mHomeModulesStateListener);
        mModuleRegistry = moduleRegistry;

        assert mModuleRegistry != null;

        mHomeModulesContextMenuManager =
                new HomeModulesContextMenuManager(
                        this,
                        moduleDelegateHost.getContextMenuStartPoint(),
                        mHomeModulesConfigManager);
        mProfileSupplier = profileSupplier;

        mModel = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModel);

        mModuleRegistry.registerAdapter(mAdapter, this::onViewCreated);
        mRecyclerView = parentView.findViewById(R.id.home_modules_recycler_view);

        mRecyclerView.setAdapter(mAdapter);
        LinearLayoutManager linearLayoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        mRecyclerView.setLayoutManager(linearLayoutManager);

        // Add pager indicator.
        setupRecyclerView(activity);

        mOnScrollListener =
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        super.onScrolled(recyclerView, dx, dy);
                        if (dx != 0) {
                            mHasHomeModulesBeenScrolled = true;
                            recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ true);
                        }
                    }
                };

        mMediator = new HomeModulesMediator(mModel, moduleRegistry);
    }

    private void setupRecyclerView(Activity activity) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
        int startMargin = mModuleDelegateHost.getStartMargin();
        mUiConfig = isTablet ? mModuleDelegateHost.getUiConfig() : null;
        mItemPerScreen =
                mUiConfig == null
                        ? 1
                        : CirclePagerIndicatorDecoration.getItemPerScreen(
                                mUiConfig.getCurrentDisplayStyle());
        mRecyclerView.initialize(isTablet, startMargin, mItemPerScreen);

        mPageIndicatorDecoration =
                new CirclePagerIndicatorDecoration(
                        activity,
                        startMargin,
                        SemanticColorUtils.getDefaultIconColorSecondary(activity),
                        activity.getColor(
                                org.chromium.components.browser_ui.styles.R.color
                                        .color_primary_with_alpha_15),
                        isTablet);
        mRecyclerView.addItemDecoration(mPageIndicatorDecoration);
        mSnapHelper =
                new PagerSnapHelper() {
                    @Override
                    public void attachToRecyclerView(@Nullable RecyclerView recyclerView)
                            throws IllegalStateException {
                        super.attachToRecyclerView(recyclerView);
                        mIsSnapHelperAttached = recyclerView != null;
                    }
                };

        // Snap scroll is supported by the recyclerview if it shows a single item per screen. This
        // happens on phones or small windows on tablets.
        if (!isTablet) {
            mSnapHelper.attachToRecyclerView(mRecyclerView);
            return;
        }

        mItemPerScreen =
                CirclePagerIndicatorDecoration.getItemPerScreen(mUiConfig.getCurrentDisplayStyle());
        if (mItemPerScreen == 1) {
            mSnapHelper.attachToRecyclerView(mRecyclerView);
        }

        // Setup an observer of mUiConfig on tablets.
        mDisplayStyleObserver =
                newDisplayStyle -> {
                    mItemPerScreen =
                            CirclePagerIndicatorDecoration.getItemPerScreen(newDisplayStyle);
                    if (mItemPerScreen > 1) {
                        // If showing multiple items per screen, we need to detach the snap
                        // scroll helper from the recyclerview.
                        if (mIsSnapHelperAttached) {
                            mSnapHelper.attachToRecyclerView(null);
                        }
                    } else if (!mIsSnapHelperAttached) {
                        // If showing a single item per screen and we haven't attached the snap
                        // scroll helper to the recyclerview yet, attach it now.
                        mSnapHelper.attachToRecyclerView(mRecyclerView);
                    }

                    // Notifies the CirclePageIndicatorDecoration.
                    int updatedStartMargin = mModuleDelegateHost.getStartMargin();
                    mPageIndicatorDecoration.onDisplayStyleChanged(
                            updatedStartMargin, mItemPerScreen);
                    mRecyclerView.onDisplayStyleChanged(updatedStartMargin, mItemPerScreen);

                    // Redraws the recyclerview when display style is changed on tablets.
                    mRecyclerView.invalidateItemDecorations();
                };
        mUiConfig.addObserver(mDisplayStyleObserver);
        mPageIndicatorDecoration.onDisplayStyleChanged(startMargin, mItemPerScreen);
        mRecyclerView.onDisplayStyleChanged(startMargin, mItemPerScreen);
    }

    /**
     * Gets the module ranking list and shows the home modules.
     *
     * @param onHomeModulesShownCallback The callback called when the magic stack is shown.
     */
    public void show(Callback<Boolean> onHomeModulesShownCallback) {
        if (mOnProfileAvailableObserver != null) {
            // If the magic stack is waiting for the profile and show() is called again, early
            // return here since showing is working in progress.
            return;
        }

        if (mProfileSupplier.hasValue()) {
            showImpl(onHomeModulesShownCallback);
        } else {
            long waitForProfileStartTimeMs = SystemClock.elapsedRealtime();
            mOnProfileAvailableObserver =
                    (profile) -> {
                        onProfileAvailable(
                                profile, onHomeModulesShownCallback, waitForProfileStartTimeMs);
                    };

            mProfileSupplier.addObserver(mOnProfileAvailableObserver);
        }
    }

    /** Shows the magic stack with profile ready. */
    private void showImpl(Callback<Boolean> onHomeModulesShownCallback) {
        // Initializing segmentation service since profile is available.
        assert mProfileSupplier.hasValue();
        mSegmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(mProfileSupplier.get());
        if (mSegmentationPlatformService == null
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER)) {
            onGotRankedModules(
                    getFixedModuleList(), onHomeModulesShownCallback, /* durationMs= */ 0);
            return;
        }
        getSegmentationRanking(onHomeModulesShownCallback);
    }

    private void onProfileAvailable(
            Profile profile,
            Callback<Boolean> onHomeModulesShownCallback,
            long waitForProfileStartTimeMs) {
        long delay = SystemClock.elapsedRealtime() - waitForProfileStartTimeMs;
        showImpl(onHomeModulesShownCallback);

        mProfileSupplier.removeObserver(mOnProfileAvailableObserver);
        mOnProfileAvailableObserver = null;
        HomeModulesMetricsUtils.recordProfileReadyDelay(getHostSurfaceType(), delay);
    }

    /** Reacts when the home modules' specific module type is disabled or enabled. */
    void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled) {
        if (isEnabled) {
            // If the mEnabledModuleSet hasn't been initialized yet, skip here.
            if (mEnabledModuleSet != null) {
                mEnabledModuleSet.add(moduleType);
            }
        } else {
            // If the mEnabledModuleSet hasn't been initialized yet, skip here.
            if (mEnabledModuleSet != null) {
                mEnabledModuleSet.remove(moduleType);
            }
            removeModule(moduleType);
        }
    }

    /** Hides the modules and cleans up. */
    public void hide() {
        if (!mHasHomeModulesBeenScrolled) {
            recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ false);
        }
        mHasHomeModulesBeenScrolled = false;
        mMediator.hide();
    }

    // ModuleDelegate implementation.

    @Override
    public void onDataReady(@ModuleType int moduleType, @NonNull PropertyModel propertyModel) {
        mMediator.addToRecyclerViewOrCache(moduleType, propertyModel);
    }

    @Override
    public void onDataFetchFailed(int moduleType) {
        mMediator.addToRecyclerViewOrCache(moduleType, null);
    }

    @Override
    public void onUrlClicked(@NonNull GURL gurl, @ModuleType int moduleType) {
        int moduleRank = mMediator.getModuleRank(moduleType);
        mModuleDelegateHost.onUrlClicked(gurl);
        onModuleClicked(moduleType, moduleRank);
    }

    @Override
    public void onTabClicked(int tabId, @ModuleType int moduleType) {
        int moduleRank = mMediator.getModuleRank(moduleType);
        mModuleDelegateHost.onTabSelected(tabId);
        onModuleClicked(moduleType, moduleRank);
    }

    @Override
    public void onModuleClicked(@ModuleType int moduleType, int modulePosition) {
        int hostSurface = mModuleDelegateHost.getHostSurfaceType();
        HomeModulesMetricsUtils.recordModuleClickedPosition(
                hostSurface, moduleType, modulePosition);
    }

    @Override
    public int getHostSurfaceType() {
        return mModuleDelegateHost.getHostSurfaceType();
    }

    @Override
    public void removeModule(@ModuleType int moduleType) {
        boolean isModuleRemoved = mMediator.remove(moduleType);

        if (isModuleRemoved && mModel.size() < mItemPerScreen) {
            mRecyclerView.invalidateItemDecorations();
        }
    }

    @Override
    public void removeModuleAndDisable(int moduleType) {
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(moduleType, false);
    }

    @Override
    public void customizeSettings() {
        mModuleDelegateHost.customizeSettings();
    }

    @Override
    public ModuleProvider getModuleProvider(int moduleType) {
        return mMediator.getModuleProvider(moduleType);
    }

    // OnViewCreatedCallback implementation.

    @Override
    public void onViewCreated(@ModuleType int moduleType, @NonNull ViewGroup group) {
        ModuleProvider moduleProvider = getModuleProvider(moduleType);
        assert moduleProvider != null;

        group.setOnLongClickListener(
                view -> {
                    Point offset = mHomeModulesContextMenuManager.getContextMenuOffset();
                    return view.showContextMenu(offset.x, offset.y);
                });
        group.setOnCreateContextMenuListener(
                (contextMenu, view, contextMenuInfo) -> {
                    mHomeModulesContextMenuManager.createContextMenu(
                            contextMenu, view, moduleProvider);
                });
        HomeModulesMetricsUtils.recordModuleShown(getHostSurfaceType(), moduleType);
    }

    /**
     * Returns the instance of the home surface {@link ModuleDelegateHost} which owns the magic
     * stack.
     */
    public ModuleDelegateHost getModuleDelegateHost() {
        return mModuleDelegateHost;
    }

    public void destroy() {
        hide();
        if (mUiConfig != null) {
            mUiConfig.removeObserver(mDisplayStyleObserver);
            mUiConfig = null;
        }
        if (mHomeModulesConfigManager != null) {
            mHomeModulesConfigManager.removeListener(mHomeModulesStateListener);
            mHomeModulesConfigManager = null;
        }
    }

    public boolean getIsSnapHelperAttachedForTesting() {
        return mIsSnapHelperAttached;
    }

    /**
     * This method returns the list of enabled modules based on surface (Start/NTP). The list
     * returned is the intersection of modules that are enabled and available for the surface.
     */
    @VisibleForTesting
    List<Integer> getFixedModuleList() {
        List<Integer> generalModuleList = new ArrayList<Integer>();
        boolean addAll = HomeModulesMetricsUtils.HOME_MODULES_SHOW_ALL_MODULES.getValue();
        boolean isHomeSurface = mModuleDelegateHost.isHomeSurface();
        generalModuleList.add(ModuleType.PRICE_CHANGE);
        if (addAll || isHomeSurface) {
            generalModuleList.add(ModuleType.SINGLE_TAB);
        }
        // Make tab resumption module NTP-only.
        if (addAll
                || (!isHomeSurface && ChromeFeatureList.sTabResumptionModuleAndroid.isEnabled())) {
            generalModuleList.add(ModuleType.TAB_RESUMPTION);
        }

        ensureEnabledModuleSetCreated();
        List<Integer> moduleList = new ArrayList<>();
        for (int i = 0; i < generalModuleList.size(); i++) {
            @ModuleType int currentModuleType = generalModuleList.get(i);
            if (mEnabledModuleSet.contains(currentModuleType)) {
                moduleList.add(currentModuleType);
            }
        }
        return moduleList;
    }

    private void onGotRankedModules(
            List<Integer> moduleList,
            Callback<Boolean> onHomeModulesShownCallback,
            long durationMs) {
        // Record only if ranking is fetched from segmentation service.
        if (durationMs > 0) {
            HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(
                    getHostSurfaceType(), durationMs);
        }
        if (moduleList == null) {
            onHomeModulesShownCallback.onResult(false);
            return;
        }

        mRecyclerView.addOnScrollListener(mOnScrollListener);
        mMediator.buildModulesAndShow(
                moduleList,
                this,
                (isVisible) -> {
                    onHomeModulesShownCallback.onResult(isVisible);
                });
    }

    private void getSegmentationRanking(Callback<Boolean> onHomeModulesShownCallback) {
        PredictionOptions options = new PredictionOptions(false);
        long segmentationServiceCallTimeMs = SystemClock.elapsedRealtime();
        mSegmentationPlatformService.getClassificationResult(
                "android_home_module_ranker",
                options,
                /* inputContext= */ null,
                result -> {
                    // It is possible that the result is received after the magic stack has been
                    // hidden, exit now.
                    long durationMs = SystemClock.elapsedRealtime() - segmentationServiceCallTimeMs;
                    if (mHomeModulesConfigManager == null) {
                        HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(
                                getHostSurfaceType(), durationMs);
                        return;
                    }
                    onGotRankedModules(
                            onGetClassificationResult(result),
                            onHomeModulesShownCallback,
                            durationMs);
                });
    }

    @VisibleForTesting
    List<Integer> onGetClassificationResult(ClassificationResult result) {
        List<Integer> moduleList;
        // If segmentation service fails, fallback to return fixed module list.
        if (result.status != PredictionStatus.SUCCEEDED || result.orderedLabels.isEmpty()) {
            moduleList = getFixedModuleList();
        } else {
            moduleList = filterEnabledModuleList(result.orderedLabels);
        }
        return moduleList;
    }

    /**
     * This method gets the list of enabled modules based on surface (Start/NTP) and returns the
     * list of modules which are present in both the previous list and the module list from the
     * model.
     */
    private List<Integer> filterEnabledModuleList(List<String> orderedModuleLabels) {
        List<Integer> localEnabledModuleList = getFixedModuleList();
        List<Integer> moduleList = new ArrayList<>();
        for (String label : orderedModuleLabels) {
            @ModuleType
            int currentModuleType = HomeModulesMetricsUtils.convertLabelToModuleType(label);
            if (localEnabledModuleList.contains(currentModuleType)) {
                moduleList.add(currentModuleType);
            }
        }
        return moduleList;
    }

    /**
     * Initializes the mEnabledModuleSet if hasn't yet. The mEnabledModuleSet should only be created
     * after Profile is ready.
     */
    @VisibleForTesting
    void ensureEnabledModuleSetCreated() {
        if (mEnabledModuleSet != null) return;

        mEnabledModuleSet = mHomeModulesConfigManager.getEnabledModuleSet();
    }

    /**
     * Records whether the magic stack is scrollable and has been scrolled or not before it is
     * hidden or destroyed and remove the on scroll listener.
     */
    private void recordMagicStackScroll(boolean hasHomeModulesBeenScrolled) {
        mMediator.recordMagicStackScroll(hasHomeModulesBeenScrolled);
        mRecyclerView.removeOnScrollListener(mOnScrollListener);
    }

    void setMediatorForTesting(HomeModulesMediator mediator) {
        mMediator = mediator;
    }
}
