// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordExternalInteractions;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.ExternalInteractions;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Safety Hub Magic Stack module. */
class SafetyHubMagicStackMediator implements TabModelSelectorObserver, MagicStackBridge.Observer {
    private final Context mContext;
    private final Profile mProfile;
    private final PrefService mPrefService;
    private final PropertyModel mModel;
    private final MagicStackBridge mMagicStackBridge;
    private final TabModelSelector mTabModelSelector;
    private final ModuleDelegate mModuleDelegate;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Callback<String> mShowSurveyCallback;

    private boolean mHasBeenDismissed;

    SafetyHubMagicStackMediator(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull PrefService prefService,
            @NonNull PropertyModel model,
            @NonNull MagicStackBridge magicStackBridge,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull PrefChangeRegistrar prefChangeRegistrar,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Callback<String> showSurveyCallback) {
        mContext = context;
        mProfile = profile;
        mPrefService = prefService;
        mModel = model;
        mMagicStackBridge = magicStackBridge;
        mTabModelSelector = tabModelSelector;
        mModuleDelegate = moduleDelegate;
        mPrefChangeRegistrar = prefChangeRegistrar;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mShowSurveyCallback = showSurveyCallback;
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
                bindRevokedPermissionsView(magicStackEntry.getDescription());
                break;
            case MagicStackEntry.ModuleType.SAFE_BROWSING:
                bindSafeBrowsingView(magicStackEntry.getDescription());
                break;
            case MagicStackEntry.ModuleType.PASSWORDS:
                bindCompromisedPasswordsView(magicStackEntry.getDescription());
                break;
        }

        mModuleDelegate.onDataReady(ModuleType.SAFETY_HUB, mModel);

        // Add observers to dismiss the module if necessary.
        mMagicStackBridge.addObserver(this);
        if (magicStackEntry.getModuleType().equals(MagicStackEntry.ModuleType.SAFE_BROWSING)) {
            mPrefChangeRegistrar.addObserver(
                    Pref.SAFE_BROWSING_ENABLED, this::onSafeBrowsingChanged);
        } else if (magicStackEntry.getModuleType().equals(MagicStackEntry.ModuleType.PASSWORDS)) {
            mPrefChangeRegistrar.addObserver(
                    Pref.BREACHED_CREDENTIALS_COUNT, this::onCompromisedPasswordsCountChanged);
        }
    }

    void destroy() {
        mTabModelSelector.removeObserver(this);
        mMagicStackBridge.removeObserver(this);

        mPrefChangeRegistrar.removeObserver(Pref.SAFE_BROWSING_ENABLED);
        mPrefChangeRegistrar.removeObserver(Pref.BREACHED_CREDENTIALS_COUNT);
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

    private void onCompromisedPasswordsCountChanged() {
        boolean hasCompromisedPasswords =
                mPrefService.getInteger(Pref.BREACHED_CREDENTIALS_COUNT) > 0;
        if (!hasCompromisedPasswords && !mHasBeenDismissed) {
            mMagicStackBridge.dismissCompromisedPasswordsModule();
            dismissModule();
        }
    }

    private void dismissModule() {
        mHasBeenDismissed = true;
        mModuleDelegate.removeModule(ModuleType.SAFETY_HUB);
    }

    private void bindRevokedPermissionsView(@NonNull String title) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(SafetyHubMagicStackViewProperties.TITLE, title);
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
                    mShowSurveyCallback.onResult(MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, SafetyHubFragment.class);
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
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mShowSurveyCallback.onResult(
                            MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, SafetyHubFragment.class);
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
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mShowSurveyCallback.onResult(MagicStackEntry.ModuleType.SAFE_BROWSING);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, SafeBrowsingSettingsFragment.class);
                    recordExternalInteractions(
                            ExternalInteractions.OPEN_SAFE_BROWSING_FROM_MAGIC_STACK);
                    mMagicStackBridge.dismissSafeBrowsingModule();
                    dismissModule();
                });
    }

    private void bindCompromisedPasswordsView(String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_password_manager_key,
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mShowSurveyCallback.onResult(MagicStackEntry.ModuleType.PASSWORDS);
                    SafetyHubUtils.showPasswordCheckUI(
                            mContext, mProfile, mModalDialogManagerSupplier);
                    recordExternalInteractions(ExternalInteractions.OPEN_GPM_FROM_MAGIC_STACK);
                    mMagicStackBridge.dismissCompromisedPasswordsModule();
                    dismissModule();
                });
    }
}
