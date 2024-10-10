// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.app.Activity;
import android.content.Context;
import android.graphics.Point;
import android.os.SystemClock;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.SnapHelper;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry.OnViewCreatedCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.Set;

/** Root coordinator which is responsible for showing modules on home surfaces. */
public class HomeModulesCoordinator implements ModuleDelegate, OnViewCreatedCallback {
    public static int MAXIMUM_MODULE_SIZE = 5;
    private final Context mContext;
    private final ModuleDelegateHost mModuleDelegateHost;
    private HomeModulesMediator mMediator;
    private final HomeModulesRecyclerView mRecyclerView;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ModuleRegistry mModuleRegistry;

    private ModelList mModel;
    private HomeModulesContextMenuManager mHomeModulesContextMenuManager;
    private SimpleRecyclerViewAdapter mAdapter;
    private CirclePagerIndicatorDecoration mPageIndicatorDecoration;
    private SnapHelper mSnapHelper;
    private boolean mIsSnapHelperAttached;
    private int mItemPerScreen;
    private HomeModulesConfigManager mHomeModulesConfigManager;
    private HomeModulesConfigManager.HomeModulesStateListener mHomeModulesStateListener;

    /** It is non-null for tablets. */
    @Nullable private UiConfig mUiConfig;

    /** It is non-null for tablets. */
    @Nullable private DisplayStyleObserver mDisplayStyleObserver;

    @Nullable private Callback<Profile> mOnProfileAvailableObserver;
    private boolean mHasHomeModulesBeenScrolled;
    private RecyclerView.OnScrollListener mOnScrollListener;
    private CallbackController mCallbackController;

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
        mContext = activity;
        mModuleDelegateHost = moduleDelegateHost;
        mHomeModulesConfigManager = homeModulesConfigManager;
        mHomeModulesStateListener = this::onModuleConfigChanged;
        mHomeModulesConfigManager.addListener(mHomeModulesStateListener);
        mModuleRegistry = moduleRegistry;

        assert mModuleRegistry != null;

        mCallbackController = new CallbackController();
        mHomeModulesContextMenuManager =
                new HomeModulesContextMenuManager(
                        this, moduleDelegateHost.getContextMenuStartPoint());
        mProfileSupplier = profileSupplier;

        mModel = new ModelList();
        mRecyclerView = parentView.findViewById(R.id.home_modules_recycler_view);
        LinearLayoutManager linearLayoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        mRecyclerView.setLayoutManager(linearLayoutManager);

        maybeSetUpAdapter();
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

        mMediator =
                new HomeModulesMediator(
                        mModel, moduleRegistry, mModuleDelegateHost, mHomeModulesConfigManager);
    }

    // Creates an Adapter and attaches it to the recyclerview if it hasn't yet.
    private void maybeSetUpAdapter() {
        if (mAdapter != null) return;

        mAdapter =
                new SimpleRecyclerViewAdapter(mModel) {
                    @Override
                    public void onViewRecycled(ViewHolder holder) {
                        holder.itemView.setOnLongClickListener(null);
                        holder.itemView.setOnCreateContextMenuListener(null);
                        super.onViewRecycled(holder);
                    }
                };
        mModuleRegistry.registerAdapter(mAdapter, this);
        mRecyclerView.setAdapter(mAdapter);
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
        Runnable callback = createOnModuleChangedCallback(onHomeModulesShownCallback);

        if (mOnProfileAvailableObserver != null) {
            // If the magic stack is waiting for the profile and show() is called again, early
            // return here since showing is working in progress.
            return;
        }

        if (mProfileSupplier.hasValue()) {
            mMediator.showModules(callback, this, getSegmentationPlatformService());
        } else {
            long waitForProfileStartTimeMs = SystemClock.elapsedRealtime();
            mOnProfileAvailableObserver =
                    (profile) -> {
                        onProfileAvailable(callback, waitForProfileStartTimeMs);
                    };

            mProfileSupplier.addObserver(mOnProfileAvailableObserver);
        }
    }

    @VisibleForTesting
    Runnable createOnModuleChangedCallback(Callback<Boolean> onHomeModulesShownCallback) {
        Runnable callback =
                mCallbackController.makeCancelable(
                        () -> {
                            int size = mModel.size();
                            if (size == 1) {
                                onHomeModulesShownCallback.onResult(true);
                            } else if (size == 0) {
                                onHomeModulesShownCallback.onResult(false);
                            }
                            // Invalidates the page indication when the RecyclerView becomes
                            // visible.
                            if (size > mItemPerScreen) {
                                mRecyclerView.invalidateItemDecorations();
                            }
                        });
        return callback;
    }

    private void onProfileAvailable(
            Runnable onHomeModulesChangedCallback, long waitForProfileStartTimeMs) {
        long delay = SystemClock.elapsedRealtime() - waitForProfileStartTimeMs;
        mMediator.showModules(onHomeModulesChangedCallback, this, getSegmentationPlatformService());

        mProfileSupplier.removeObserver(mOnProfileAvailableObserver);
        mOnProfileAvailableObserver = null;
        HomeModulesMetricsUtils.recordProfileReadyDelay(delay);
    }

    /** Reacts when the home modules' specific module type is disabled or enabled. */
    void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled) {
        // Updates the enabled module list.
        mMediator.onModuleConfigChanged(moduleType, isEnabled);

        // The single tab module and the tab resumption modules are controlled by the same
        // preference key. Once it is turned on or off, both modules will be enabled or disabled.
        if (!isEnabled) {
            removeModule(moduleType);
            if (moduleType == ModuleType.SINGLE_TAB) {
                removeModule(ModuleType.TAB_RESUMPTION);
            } else if (moduleType == ModuleType.TAB_RESUMPTION) {
                removeModule(ModuleType.SINGLE_TAB);
            }
        }
    }

    /** Hides the modules and cleans up. */
    public void hide() {
        if (!mHasHomeModulesBeenScrolled) {
            recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ false);
        }
        mHasHomeModulesBeenScrolled = false;
        mMediator.hide();

        destroyAdapter();
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
        HomeModulesMetricsUtils.recordModuleClicked(
                moduleType, modulePosition, mModuleDelegateHost.isHomeSurface());
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

    @Override
    public Tab getTrackingTab() {
        return mModuleDelegateHost.getTrackingTab();
    }

    @Override
    public void prepareBuildAndShow() {
        maybeSetUpAdapter();
        mRecyclerView.addOnScrollListener(mOnScrollListener);
    }

    // OnViewCreatedCallback implementation.

    @Override
    public void onViewCreated(@ModuleType int moduleType, @NonNull ViewGroup group) {
        ModuleProvider moduleProvider = getModuleProvider(moduleType);
        assert moduleProvider != null;

        LayoutParams layoutParams = group.getLayoutParams();
        layoutParams.height =
                mContext.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.magic_stack.R.dimen.home_module_height);
        group.setLayoutParams(layoutParams);
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
        int position = mMediator.findModuleIndexInRecyclerView(moduleType, mAdapter.getItemCount());
        HomeModulesMetricsUtils.recordModuleShown(
                moduleType, position, mModuleDelegateHost.isHomeSurface());
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
        if (mHomeModulesContextMenuManager != null) {
            mHomeModulesContextMenuManager.destroy();
            mHomeModulesContextMenuManager = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    public boolean getIsSnapHelperAttachedForTesting() {
        return mIsSnapHelperAttached;
    }

    /**
     * Records whether the magic stack is scrollable and has been scrolled or not before it is
     * hidden or destroyed and remove the on scroll listener.
     */
    private void recordMagicStackScroll(boolean hasHomeModulesBeenScrolled) {
        mMediator.recordMagicStackScroll(hasHomeModulesBeenScrolled);
        mRecyclerView.removeOnScrollListener(mOnScrollListener);
    }

    private void destroyAdapter() {
        if (mAdapter == null) return;

        // Destroys and unattaches the adapter to allow recycling the views.
        mRecyclerView.setAdapter(null);
        mAdapter.destroy();
        mAdapter = null;
    }

    private SegmentationPlatformService getSegmentationPlatformService() {
        return SegmentationPlatformServiceFactory.getForProfile(mProfileSupplier.get());
    }

    void setMediatorForTesting(HomeModulesMediator mediator) {
        mMediator = mediator;
    }

    Set<Integer> getFilteredEnabledModuleSetForTesting() {
        return mMediator.getFilteredEnabledModuleSet();
    }

    public HomeModulesContextMenuManager getHomeModulesContextMenuManagerForTesting() {
        return mHomeModulesContextMenuManager;
    }

    public void setModelForTesting(ModelList model) {
        mModel = model;
    }
}
