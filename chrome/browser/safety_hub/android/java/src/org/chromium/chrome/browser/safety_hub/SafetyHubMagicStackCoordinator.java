// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the Safety Hub Magic Stack module. */
class SafetyHubMagicStackCoordinator implements ModuleProvider {
    private final SafetyHubMagicStackMediator mMediator;

    SafetyHubMagicStackCoordinator(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Callback<String> showSurveyCallback) {
        PropertyModel model = new PropertyModel(SafetyHubMagicStackViewProperties.ALL_KEYS);
        mMediator =
                new SafetyHubMagicStackMediator(
                        context,
                        profile,
                        UserPrefs.get(profile),
                        model,
                        MagicStackBridge.getForProfile(profile),
                        tabModelSelector,
                        moduleDelegate,
                        new PrefChangeRegistrar(),
                        modalDialogManagerSupplier,
                        showSurveyCallback);
    }

    @Override
    public void showModule() {
        mMediator.showModule();
    }

    @Override
    public void hideModule() {
        mMediator.destroy();
    }

    @Override
    public int getModuleType() {
        return mMediator.getModuleType();
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return context.getString(R.string.safety_hub_magic_stack_hide_text);
    }
}
