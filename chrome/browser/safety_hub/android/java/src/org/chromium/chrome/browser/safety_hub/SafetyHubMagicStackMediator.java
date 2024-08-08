// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordExternalInteractions;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.ExternalInteractions;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Safety Hub Magic Stack module. */
class SafetyHubMagicStackMediator implements TabModelSelectorObserver, MagicStackBridge.Observer {
    private final Context mContext;
    private final PrefService mPrefService;
    private final PropertyModel mModel;
    private final MagicStackBridge mMagicStackBridge;
    private final TabModelSelector mTabModelSelector;
    private final ModuleDelegate mModuleDelegate;
    private final SettingsLauncher mSettingsLauncher;
    private final PrefChangeRegistrar mPrefChangeRegistrar;

    private boolean mHasBeenDismissed;

    SafetyHubMagicStackMediator(
            Context context,
            PrefService prefService,
            PropertyModel model,
            MagicStackBridge magicStackBridge,
            TabModelSelector tabModelSelector,
            ModuleDelegate moduleDelegate,
            SettingsLauncher settingsLauncher,
            PrefChangeRegistrar prefChangeRegistrar) {
        mContext = context;
        mPrefService = prefService;
        mModel = model;
        mMagicStackBridge = magicStackBridge;
        mTabModelSelector = tabModelSelector;
        mModuleDelegate = moduleDelegate;
        mSettingsLauncher = settingsLauncher;
        mPrefChangeRegistrar = prefChangeRegistrar;
    }

    void showModule() {
        if (mHasBeenDismissed) {
            return;
        }

        if (!mTabModelSelector.isTabStateInitialized()) {
            mTabModelSelector.addObserver(this);
            return;
        }

        MagicStackEntry magicStackEntry = mMagicStackBridge.getModuleToShow();

        if (magicStackEntry == null) {
            mModuleDelegate.onDataFetchFailed(ModuleType.SAFETY_HUB);
            return;
        }

        switch (magicStackEntry.getModuleType()) {
            case MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS:
                bindNotificationReviewView(magicStackEntry.getDescription());
                break;
            case MagicStackEntry.ModuleType.REVOKED_PERMISSIONS:
                bindSafeStateView(magicStackEntry.getDescription(), null);
                break;
            case MagicStackEntry.ModuleType.SAFE_BROWSING:
                bindSafeBrowsingView(magicStackEntry.getDescription());
                break;
        }

        mModuleDelegate.onDataReady(ModuleType.SAFETY_HUB, mModel);

        // Add observers to dismiss the module if necessary.
        mMagicStackBridge.addObserver(this);
        if (magicStackEntry.getModuleType().equals(MagicStackEntry.ModuleType.SAFE_BROWSING)) {
            mPrefChangeRegistrar.addObserver(
                    Pref.SAFE_BROWSING_ENABLED, this::onSafeBrowsingChanged);
        }
    }

    void destroy() {
        mTabModelSelector.removeObserver(this);
        mMagicStackBridge.removeObserver(this);

        mPrefChangeRegistrar.removeObserver(Pref.SAFE_BROWSING_ENABLED);
        mPrefChangeRegistrar.destroy();
    }

    int getModuleType() {
        return ModuleType.SAFETY_HUB;
    }

    @Override
    public void activeModuleDismissed() {
        if (!mHasBeenDismissed) {
            dismissModule();
        }
    }

    @Override
    public void onTabStateInitialized() {
        mTabModelSelector.removeObserver(this);
        showModule();
    }

    private void onSafeBrowsingChanged() {
        boolean isSafeBrowsingEnabled = mPrefService.getBoolean(Pref.SAFE_BROWSING_ENABLED);
        if (isSafeBrowsingEnabled && !mHasBeenDismissed) {
            mMagicStackBridge.dismissSafeBrowsingModule();
            dismissModule();
        }
    }

    private void dismissModule() {
        mHasBeenDismissed = true;
        mModuleDelegate.removeModule(ModuleType.SAFETY_HUB);
    }

    private void bindSafeStateView(@NonNull String title, @Nullable String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(SafetyHubMagicStackViewProperties.TITLE, title);
        if (summary != null) {
            mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        }
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_check_circle_filled_green_24dp,
                        R.color.default_green));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mSettingsLauncher.launchSettingsActivity(mContext, SafetyHubFragment.class);
                    recordExternalInteractions(ExternalInteractions.OPEN_FROM_MAGIC_STACK);
                });
    }

    private void bindNotificationReviewView(@NonNull String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_notifications_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.safety_hub_notifications_icon,
                        R.color.default_icon_color_accent1_baseline));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mSettingsLauncher.launchSettingsActivity(mContext, SafetyHubFragment.class);
                    recordExternalInteractions(ExternalInteractions.OPEN_FROM_MAGIC_STACK);
                });
    }

    private void bindSafeBrowsingView(@NonNull String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_gshield_24,
                        R.color.default_icon_color_accent1_baseline));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mSettingsLauncher.launchSettingsActivity(
                            mContext, SafeBrowsingSettingsFragment.class);
                    recordExternalInteractions(
                            ExternalInteractions.OPEN_SAFE_BROWSING_FROM_MAGIC_STACK);
                });
    }
}
