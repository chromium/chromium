// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/** A class which is responsible for registering module builders {@link ModuleProviderBuilder}. */
@NullMarked
public class ModuleRegistry {
    /** The callback interface which is called when the view of a module is inflated. */
    public interface OnViewCreatedCallback {
        void onViewCreated(@ModuleType int moduleType, ViewGroup view);
    }

    private static final String TAG = "ModuleRegistry";

    /** A map of <ModuleType, ModuleProviderBuilder>. */
    private final Map<Integer, ModuleProviderBuilder> mModuleBuildersMap = new HashMap<>();

    private final HomeModulesConfigManager mHomeModulesConfigManager;

    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private LifecycleObserver mLifecycleObserver;

    public ModuleRegistry(
            HomeModulesConfigManager homeModulesConfigManager,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mHomeModulesConfigManager = homeModulesConfigManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mLifecycleObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {}

                    @Override
                    public void onPauseWithNative() {
                        for (ModuleProviderBuilder builder : mModuleBuildersMap.values()) {
                            builder.onPauseWithNative();
                        }
                    }
                };
        mActivityLifecycleDispatcher.register(mLifecycleObserver);
    }

    /**
     * Registers the builder {@link ModuleProviderBuilder} for a given module type.
     *
     * @param moduleType The type of the module.
     * @param builder The object of the module builder.
     */
    public void registerModule(@ModuleType int moduleType, ModuleProviderBuilder builder) {
        mModuleBuildersMap.put(moduleType, builder);
    }

    /**
     * Returns the set which contains all the module types that are registered and enabled according
     * to user preference. Note: this function should be called after profile is ready.
     */
    @ModuleType
    public Set<Integer> getEnabledModuleSet() {
        @ModuleType Set<Integer> enabledModuleList = new HashSet<>();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
                && !mHomeModulesConfigManager.getPrefAllCardsEnabled()) {
            return enabledModuleList;
        }

        for (Entry<Integer, ModuleProviderBuilder> entry : mModuleBuildersMap.entrySet()) {
            ModuleProviderBuilder builder = entry.getValue();
            if (builder.isEligible()
                    && mHomeModulesConfigManager.getPrefModuleTypeEnabled(entry.getKey())) {
                enabledModuleList.add(entry.getKey());
            }
        }
        return enabledModuleList;
    }

    /** Returns a list of modules that allow users to configure in settings. */
    @ModuleType
    public List<Integer> getModuleListShownInSettings() {
        @ModuleType List<Integer> moduleListShownInSettings = new ArrayList<>();
        boolean isEducationalTipModuleAdded = false;

        for (Entry<Integer, ModuleProviderBuilder> entry : mModuleBuildersMap.entrySet()) {
            ModuleProviderBuilder builder = entry.getValue();
            if (builder.isEligible()) {
                int moduleType = entry.getKey();
                if (HomeModulesUtils.belongsToEducationalTipModule(moduleType)) {
                    // All the educational tip modules are controlled by the same preference.
                    if (isEducationalTipModuleAdded) continue;

                    isEducationalTipModuleAdded = true;
                }

                moduleListShownInSettings.add(moduleType);
            }
        }
        return moduleListShownInSettings;
    }

    /**
     * Registers the existing modules to a given adapter {@link SimpleRecyclerViewAdapter}.
     *
     * @param adapter The adaptor of the given RecyclerView object.
     * @param onViewCreatedCallback The callback to notify the caller after the view of the module
     *     is created.
     */
    public void registerAdapter(
            SimpleRecyclerViewAdapter adapter, OnViewCreatedCallback onViewCreatedCallback) {
        for (Integer moduleType : mModuleBuildersMap.keySet()) {
            ModuleProviderBuilder builder = mModuleBuildersMap.get(moduleType);
            adapter.registerType(
                    moduleType,
                    parent -> {
                        ViewGroup group = builder.createView(parent);
                        onViewCreatedCallback.onViewCreated(moduleType, group);
                        return group;
                    },
                    (model, view, propertyKey) -> builder.bind(model, view, propertyKey));
        }
    }

    /**
     * Builds a module instance. The newly created module instance {@link ModuleProvider} will be
     * return to the caller in onModuleBuiltCallback.
     *
     * @param moduleType The type of the module to build.
     * @param moduleDelegate The instance of the magic stack {@link ModuleDelegate}.
     * @param onModuleBuiltCallback The callback called after the module is built.
     * @return Whether a module is built successfully.
     */
    public boolean build(
            @ModuleType int moduleType,
            ModuleDelegate moduleDelegate,
            Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!mModuleBuildersMap.containsKey(moduleType)) {
            Log.i(TAG, "The module type isn't supported!");
            return false;
        }

        ModuleProviderBuilder builder = mModuleBuildersMap.get(moduleType);
        return builder.build(moduleDelegate, onModuleBuiltCallback);
    }

    /**
     * Returns the builder {@link ModuleProviderBuilder} for a given module type.
     *
     * @param moduleType The type of the module.
     * @return The object of the module builder.
     */
    public ModuleProviderBuilder getModuleProviderBuilder(@ModuleType int moduleType) {
        assert mModuleBuildersMap.containsKey(moduleType);
        return mModuleBuildersMap.get(moduleType);
    }

    /** Returns a list of all registered module types. */
    public List<Integer> getAllRegisteredModuleTypes() {
        return new ArrayList<>(mModuleBuildersMap.keySet());
    }

    /** Destroys the registry. */
    @SuppressWarnings("NullAway") // Restrict non-@Nullable assumptions to before destroy().
    public void destroy() {
        if (mActivityLifecycleDispatcher == null) return;

        for (ModuleProviderBuilder builder : mModuleBuildersMap.values()) {
            builder.destroy();
        }
        mModuleBuildersMap.clear();
        mActivityLifecycleDispatcher.unregister(mLifecycleObserver);
        mLifecycleObserver = null;
        mActivityLifecycleDispatcher = null;
    }
}
