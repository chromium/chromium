// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The builder interface to build a module and its view. */
public interface ModuleProviderBuilder {
    /**
     * Builds a module {@link ModuleProvider}. The module will be returned to the caller in the
     * onModuleBuiltCallback.
     *
     * @param moduleDelegate The magic stack which owns the module.
     * @param onModuleBuiltCallback The callback which is called when the module is built.
     * @return Whether a module is built successfully.
     */
    boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback);

    /**
     * Creates a view for the module.
     *
     * @param parentView The parent view which holds the view of the module.
     * @return The view created.
     */
    ViewGroup createView(@NonNull ViewGroup parentView);

    /**
     * Binds the given model and the view.
     *
     * @param model The instance of the {@link PropertyModel}.
     * @param view The instance of the {@link ViewGroup}.
     * @param propertyKey The {@link PropertyKey} to handle by the view.
     */
    void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey);

    /**
     * Returns whether the module can be built due to special restrictions, like location. This
     * method should be called after profile is initialized.
     */
    default boolean isEligible() {
        return true;
    }
}
