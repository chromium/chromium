// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordExternalInteractions;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.ExternalInteractions;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Safety Hub Magic Stack module. */
@NullMarked
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
    private final SafetyHubHatsHelper mHatsHelper;

    private boolean mHasBeenDismissed;

    SafetyHubMagicStackMediator(
            Context context,
            Profile profile,
            PrefService prefService,
            PropertyModel model,
            MagicStackBridge magicStackBridge,
            TabModelSelector tabModelSelector,
            ModuleDelegate moduleDelegate,
            PrefChangeRegistrar prefChangeRegistrar,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            SafetyHubHatsHelper hatsHelper) {
        mContext = context;
        mProfile = profile;
        mPrefService = prefService;
        mModel = model;
        mMagicStackBridge = magicStackBridge;
        mTabModelSelector = tabModelSelector;
        mModuleDelegate = moduleDelegate;
        mPrefChangeRegistrar = prefChangeRegistrar;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mHatsHelper = hatsHelper;
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

        mHatsHelper.triggerProactiveHatsSurveyWhenCardShown(
                mTabModelSelector, magicStackEntry.getModuleType());

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

    private void bindRevokedPermissionsView(String title) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(SafetyHubMagicStackViewProperties.TITLE, title);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_CONTENT_DESCRIPTION,
                mContext.getString(
                        R.string.safety_hub_magic_stack_safe_state_button_content_description));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_check_circle_filled_green_24dp,
                        R.color.default_green));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                            mTabModelSelector, MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, SafetyHubFragment.class);
                    recordExternalInteractions(ExternalInteractions.OPEN_FROM_MAGIC_STACK);
                });
    }

    private void bindNotificationReviewView(String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getString(R.string.safety_hub_magic_stack_notifications_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_CONTENT_DESCRIPTION,
                mContext.getString(
                        R.string.safety_hub_magic_stack_safe_state_button_content_description));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.safety_hub_notifications_icon,
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                            mTabModelSelector, MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, SafetyHubFragment.class);
                    recordExternalInteractions(ExternalInteractions.OPEN_FROM_MAGIC_STACK);
                });
    }

    private void bindSafeBrowsingView(String summary) {
        mModel.set(
                SafetyHubMagicStackViewProperties.HEADER,
                mContext.getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getString(R.string.safety_hub_magic_stack_safe_browsing_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getString(R.string.safety_hub_magic_stack_safe_browsing_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_CONTENT_DESCRIPTION,
                mContext.getString(R.string.safety_hub_magic_stack_safe_browsing_button_text));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.secured_by_brand_shield_24,
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                            mTabModelSelector, MagicStackEntry.ModuleType.SAFE_BROWSING);
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
                mContext.getString(R.string.safety_hub_magic_stack_module_name));
        mModel.set(
                SafetyHubMagicStackViewProperties.TITLE,
                mContext.getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, summary);
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_TEXT,
                mContext.getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_CONTENT_DESCRIPTION,
                mContext.getString(R.string.safety_hub_magic_stack_compromised_passwords_title));
        mModel.set(
                SafetyHubMagicStackViewProperties.ICON_DRAWABLE,
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_password_manager_key,
                        R.color.default_icon_color_accent1_tint_list));
        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> {
                    mHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                            mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);
                    // The settingsCustomTabLauncher is only needed by dialogs shown when
                    // password manager is not available. Since the SafetyHub magic stack
                    // card is only shown if the password manager is accessible, it's fine
                    // to pass null instead.
                    SafetyHubUtils.showPasswordCheckUi(
                            mContext,
                            mProfile,
                            mModalDialogManagerSupplier,
                            /* settingsCustomTabLauncher= */ null);
                    recordExternalInteractions(ExternalInteractions.OPEN_GPM_FROM_MAGIC_STACK);
                    mMagicStackBridge.dismissCompromisedPasswordsModule();
                    dismissModule();
                });
    }
}
