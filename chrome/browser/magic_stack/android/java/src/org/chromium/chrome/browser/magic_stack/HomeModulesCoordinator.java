// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.app.Activity;
import android.graphics.Point;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.SnapHelper;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry.OnViewCreatedCallback;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
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
    private final HomeModulesMediator mMediator;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final RecyclerView mRecyclerView;
    private final ModelList mModel;
    private final HomeModulesContextMenuManager mHomeModulesContextMenuManager;

    private CirclePagerIndicatorDecoration mPageIndicatorDecoration;
    private SnapHelper mSnapHelper;
    private boolean mIsSnapHelperAttached;
    private int mCurrentOrientation;
    private int mItemPerScreen;
    @Nullable private UiConfig mUiConfig;
    @Nullable private DisplayStyleObserver mDisplayStyleObserver;

    private Set<Integer> mEnabledModuleList;
    private HomeModulesConfigManager mHomeModulesConfigManager;
    private HomeModulesConfigManager.HomeModulesStateListener mHomeModulesStateListener;

    /**
     * @param activity The instance of {@link Activity}.
     * @param moduleDelegateHost The home surface which owns the magic stack.
     * @param parentView The parent view which holds the magic stack's RecyclerView.
     */
    public HomeModulesCoordinator(
            @NonNull Activity activity,
            @NonNull ModuleDelegateHost moduleDelegateHost,
            @NonNull ViewGroup parentView,
            @NonNull HomeModulesConfigManager homeModulesConfigManager) {
        mModuleDelegateHost = moduleDelegateHost;
        ModuleRegistry moduleRegistry = ModuleRegistry.getInstance();
        mHomeModulesContextMenuManager =
                new HomeModulesContextMenuManager(
                        this, moduleDelegateHost.getContextMenuStartPoint(), moduleRegistry);

        mModel = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModel);
        moduleRegistry.registerAdapter(mAdapter, this::onViewCreated);
        mRecyclerView = parentView.findViewById(R.id.home_modules_recycler_view);

        mRecyclerView.setAdapter(mAdapter);
        LinearLayoutManager linearLayoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        mRecyclerView.setLayoutManager(linearLayoutManager);

        // Add pager indicator.
        setupRecyclerView(activity);

        mHomeModulesConfigManager = homeModulesConfigManager;
        mHomeModulesStateListener = this::onModuleConfigChanged;
        mHomeModulesConfigManager.addListener(mHomeModulesStateListener);
        mEnabledModuleList = mHomeModulesConfigManager.getEnabledModuleList();

        mMediator = new HomeModulesMediator(mModel, moduleRegistry);
    }

    private void setupRecyclerView(Activity activity) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
        mUiConfig = isTablet ? mModuleDelegateHost.getUiConfig() : null;
        mPageIndicatorDecoration =
                new CirclePagerIndicatorDecoration(
                        activity,
                        mUiConfig,
                        mModuleDelegateHost.getStartMargin(),
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

        // When the screen is rotated, an event of display style change is also triggered.
        mCurrentOrientation = activity.getResources().getConfiguration().orientation;

        // Setup an observer of mUiConfig on tablets.
        mDisplayStyleObserver =
                newDisplayStyle -> {
                    boolean wasSnapHelperAttached = mIsSnapHelperAttached;
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
                    mPageIndicatorDecoration.onDisplayStyleChanged(
                            mModuleDelegateHost.getStartMargin(), mItemPerScreen);

                    int newOrientation = activity.getResources().getConfiguration().orientation;
                    // Redraws the recyclerview when either the screen is rotated or the width
                    // of the window in which the magic stack is shown has changed.
                    if (wasSnapHelperAttached != mIsSnapHelperAttached
                            || mCurrentOrientation != newOrientation) {
                        mCurrentOrientation = newOrientation;
                        // Makes the recyclerview to redraw all items.
                        mRecyclerView.invalidateItemDecorations();
                    }
                };
        mUiConfig.addObserver(mDisplayStyleObserver);
        mPageIndicatorDecoration.onDisplayStyleChanged(
                mModuleDelegateHost.getStartMargin(), mItemPerScreen);
    }

    /**
     * Gets the module ranking list and shows the home modules.
     *
     * @param onHomeModulesShownCallback The callback called when the magic stack is shown.
     */
    public void show(Callback<Boolean> onHomeModulesShownCallback) {
        List<Integer> moduleList = getModuleList();
        if (moduleList == null) {
            onHomeModulesShownCallback.onResult(false);
            return;
        }

        mMediator.buildModulesAndShow(
                moduleList,
                this,
                (isVisible) -> {
                    onHomeModulesShownCallback.onResult(isVisible);
                });
    }

    /** Reacts when the home modules' specific module type is disabled or enabled. */
    void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled) {
        if (isEnabled) {
            mEnabledModuleList.add(moduleType);
        } else {
            mEnabledModuleList.remove(moduleType);
            removeModule(moduleType);
        }
    }

    /** Hides the modules and cleans up. */
    public void hide() {
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
        mModuleDelegateHost.onUrlClicked(gurl);
        onModuleClicked(moduleType);
    }

    @Override
    public void onTabClicked(int tabId, @ModuleType int moduleType) {
        mModuleDelegateHost.onTabSelected(tabId, moduleType);
        onModuleClicked(moduleType);
    }

    @Override
    public void onModuleClicked(@ModuleType int moduleType) {
        HomeModulesMetricsUtils.recordModuleClick(
                mModuleDelegateHost.getHostSurfaceType(), moduleType);
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

    @VisibleForTesting
    List<Integer> getModuleList() {
        // TODO(https://crbug.com/1512962): Gets the modules ranking list using segmentation service
        // API.
        List<Integer> generalModuleList = List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        List<Integer> moduleList = new ArrayList<>();
        for (int i = 0; i < generalModuleList.size(); i++) {
            @ModuleType int currentModuleType = generalModuleList.get(i);
            if (mEnabledModuleList.contains(currentModuleType)) {
                moduleList.add(currentModuleType);
            }
        }
        return moduleList;
    }
}
