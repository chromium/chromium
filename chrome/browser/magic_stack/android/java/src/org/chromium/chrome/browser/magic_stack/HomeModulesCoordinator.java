// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.app.Activity;
import android.graphics.Point;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.magic_stack.ModuleRegistry.OnViewCreatedCallback;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.List;

/** Root coordinator which is responsible for showing modules on home surfaces. */
public class HomeModulesCoordinator implements ModuleDelegate, OnViewCreatedCallback {
    private final ModuleDelegateHost mModuleDelegateHost;
    private final HomeModulesMediator mMediator;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final ModelList mModel;
    private final HomeModulesContextMenuManager mHomeModulesContextMenuManager;

    /**
     * @param activity The instance of {@link Activity}.
     * @param moduleDelegateHost The home surface which owns the magic stack.
     * @param parentView The parent view which holds the magic stack's RecyclerView.
     */
    public HomeModulesCoordinator(
            @NonNull Activity activity,
            @NonNull ModuleDelegateHost moduleDelegateHost,
            @NonNull ViewGroup parentView) {
        mModuleDelegateHost = moduleDelegateHost;
        mHomeModulesContextMenuManager =
                new HomeModulesContextMenuManager(
                        this, moduleDelegateHost.getContextMenuStartPoint());

        mModel = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModel);
        ModuleRegistry.getInstance().registerAdapter(mAdapter, this::onViewCreated);
        RecyclerView recyclerView = parentView.findViewById(R.id.home_modules_recycler_view);

        recyclerView.setAdapter(mAdapter);
        recyclerView.setHasFixedSize(true);
        LinearLayoutManager linearLayoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        recyclerView.setLayoutManager(linearLayoutManager);

        // Add pager indicator.
        recyclerView.addItemDecoration(
                new CirclePagerIndicatorDecoration(
                        activity,
                        moduleDelegateHost.getUiConfig(),
                        moduleDelegateHost.getStartMargin(),
                        SemanticColorUtils.getDefaultIconColorSecondary(activity),
                        activity.getColor(
                                org.chromium.components.browser_ui.styles.R.color
                                        .color_primary_with_alpha_15),
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)));
        PagerSnapHelper snapHelper = new PagerSnapHelper();
        snapHelper.attachToRecyclerView(recyclerView);

        mMediator =
                new HomeModulesMediator(
                        mModel,
                        (isVisible) -> {
                            recyclerView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
                        },
                        ModuleRegistry.getInstance());
    }

    /** Gets the module ranking list and shows the home modules. */
    public void show() {
        List<Integer> moduleList = getModuleList();
        if (moduleList == null) return;

        mMediator.buildModulesAndShow(moduleList, this);
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
    public void onTabClicked(int tabId, int moduleType) {
        mModuleDelegateHost.onTabSelected(tabId);
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
    public void onHideModuleFromContextMenu(@ModuleType int moduleType) {
        mMediator.remove(moduleType);
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

    private List<Integer> getModuleList() {
        // TODO(https://crbug.com/1512962): Gets the modules ranking list using segmentation service
        // API.
        return List.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
    }
}
