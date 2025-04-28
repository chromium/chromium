// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.ClickInfo;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the auxiliary search opt in module. */
@NullMarked
public class AuxiliarySearchModuleMediator {
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;
    private final Runnable mOpenSettingsRunnable;
    private final boolean mIsShareTabsDefaultEnabledByOs;

    /** Whether the module is currently showing. */
    private boolean mIsShown;

    /**
     * @param model The instance of {@link PropertyModel} for the view.
     * @param moduleDelegate The instance of {@link ModuleDelegate}, which is the magic stack.
     * @param openSettingsRunnable The runnable to open the app's settings.
     */
    public AuxiliarySearchModuleMediator(
            PropertyModel model, ModuleDelegate moduleDelegate, Runnable openSettingsRunnable) {
        mModel = model;
        mModuleDelegate = moduleDelegate;
        mOpenSettingsRunnable = openSettingsRunnable;

        mIsShareTabsDefaultEnabledByOs = AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled();

        if (mIsShareTabsDefaultEnabledByOs) {
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID,
                    R.string.auxiliary_search_module_content);
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID,
                    R.string.auxiliary_search_module_button_go_to_settings);
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID,
                    R.string.auxiliary_search_module_button_got_it);
        } else {
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID,
                    R.string.auxiliary_search_module_content_default_off);
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID,
                    R.string.auxiliary_search_module_button_no_thanks);
            mModel.set(
                    AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID,
                    R.string.auxiliary_search_module_button_turn_on);
        }
    }

    void showModule() {
        if (mIsShown) return;

        mIsShown = true;

        if (mIsShareTabsDefaultEnabledByOs) {
            setDefaultOptInCard();
        } else {
            setDefaultOptOutCard();
        }

        mModuleDelegate.onDataReady(ModuleType.AUXILIARY_SEARCH, mModel);
    }

    void hideModule() {
        mIsShown = false;

        mModel.set(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER, null);
        mModel.set(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER, null);
    }

    int getModuleType() {
        return ModuleType.AUXILIARY_SEARCH;
    }

    /** Sets the default opt in card with "Go to Settings" and "Got it" buttons. */
    private void setDefaultOptInCard() {
        mModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                view -> {
                    mOpenSettingsRunnable.run();
                    onButtonClicked(ClickInfo.OPEN_SETTINGS);
                });

        mModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                view -> {
                    onButtonClicked(ClickInfo.OPT_IN);
                });
    }

    /** Sets the default opt out card with "No thanks" and "Turn on" buttons. */
    private void setDefaultOptOutCard() {
        mModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                view -> {
                    onButtonClicked(ClickInfo.OPT_OUT);
                });

        mModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                view -> {
                    AuxiliarySearchConfigManager.getInstance().notifyShareTabsStateChanged(true);
                    onButtonClicked(ClickInfo.TURN_ON);
                });
    }

    private void onButtonClicked(@ClickInfo int type) {
        // onModuleClicked() should be called before removing the module.
        mModuleDelegate.onModuleClicked(ModuleType.AUXILIARY_SEARCH);
        mModuleDelegate.removeModule(ModuleType.AUXILIARY_SEARCH);

        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        prefManager.writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, true);

        AuxiliarySearchMetrics.recordClickButtonInfo(type);
    }
}
