// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.HashMap;
import java.util.Map;

/** A class which is responsible for registering module builders {@link ModuleProviderBuilder}. */
public class ModuleRegistry {
    /** The callback interface which is called when the view of a module is inflated. */
    public interface OnViewCreatedCallback {
        void onViewCreated(@ModuleType int moduleType, @NonNull ViewGroup view);
    }

    private static final String TAG = "ModuleRegistry";

    /** A map of <ModuleType, ModuleProviderBuilder>. */
    private final Map<Integer, ModuleProviderBuilder> mModuleBuildersMap = new HashMap<>();

    private final HomeModulesConfigManager mHomeModulesConfigManager;

    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private LifecycleObserver mLifecycleObserver;

    public ModuleRegistry(
            @NonNull HomeModulesConfigManager homeModulesConfigManager,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
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
    public void registerModule(@ModuleType int moduleType, @NonNull ModuleProviderBuilder builder) {
        mModuleBuildersMap.put(moduleType, builder);
        if (builder instanceof ModuleConfigChecker) {
            mHomeModulesConfigManager.registerModuleEligibilityChecker(
                    moduleType, (ModuleConfigChecker) builder);
        }
    }

    /**
     * Registers the existing modules to a given adapter {@link SimpleRecyclerViewAdapter}.
     *
     * @param adapter The adaptor of the given RecyclerView object.
     * @param onViewCreatedCallback The callback to notify the caller after the view of the module
     *     is created.
     */
    public void registerAdapter(
            @NonNull SimpleRecyclerViewAdapter adapter,
            @NonNull OnViewCreatedCallback onViewCreatedCallback) {
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
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!mModuleBuildersMap.containsKey(moduleType)) {
            Log.i(TAG, "The module type isn't supported!");
            return false;
        }

        ModuleProviderBuilder builder = mModuleBuildersMap.get(moduleType);
        return builder.build(moduleDelegate, onModuleBuiltCallback);
    }

    /** Destroys the registry. */
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
