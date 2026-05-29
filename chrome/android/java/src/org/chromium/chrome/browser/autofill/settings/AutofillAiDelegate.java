// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.app.Activity;
import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.List;
import java.util.Map;
import java.util.Set;

/** A delegate class to handle shared logic for Forms AI settings fragments. */
@NullMarked
public class AutofillAiDelegate {
    private static final int DEFAULT_SNACKBAR_DURATION = 10000;
    static final String DISABLED_WALLET_DATA_SHARING = "disabled_wallet_data_sharing";
    static final String DISABLED_SETTINGS_INFO = "disabled_settings_info";

    static class ToggleConfig {
        public final String key;
        public final int labelRes;
        public final int subLabelRes;
        public final String prefName;

        ToggleConfig(String key, int labelRes, int subLabelRes, String prefName) {
            this.key = key;
            this.labelRes = labelRes;
            this.subLabelRes = subLabelRes;
            this.prefName = prefName;
        }
    }

    static class AutofillAiToggleState {
        public final boolean enabled;
        public final boolean checked;
        public final boolean controlledByEnterprisePolicy;

        private AutofillAiToggleState(
                boolean enabled, boolean checked, boolean controlledByEnterprisePolicy) {
            this.enabled = enabled;
            this.checked = checked;
            this.controlledByEnterprisePolicy = controlledByEnterprisePolicy;
        }

        private static final AutofillAiToggleState DISABLED =
                new AutofillAiToggleState(
                        /* enabled= */ false,
                        /* checked= */ false,
                        /* controlledByEnterprisePolicy= */ false);
        private static final AutofillAiToggleState DISABLED_BY_POLICY =
                new AutofillAiToggleState(
                        /* enabled= */ false,
                        /* checked= */ false,
                        /* controlledByEnterprisePolicy= */ true);
        private static final AutofillAiToggleState ENABLED_ON =
                new AutofillAiToggleState(
                        /* enabled= */ true,
                        /* checked= */ true,
                        /* controlledByEnterprisePolicy= */ false);
        private static final AutofillAiToggleState ENABLED_OFF =
                new AutofillAiToggleState(
                        /* enabled= */ true,
                        /* checked= */ false,
                        /* controlledByEnterprisePolicy= */ false);
    }

    private final ChromeBaseSettingsFragment mFragment;
    private final EntityDataManager.EntityDataManagerObserver mEntityObserver;
    private @Nullable final ToggleConfig mToggleConfig;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable EntityEditorCoordinator mEntityEditor;
    private @Nullable ReauthenticatorBridge mReauthenticatorBridge;
    private final EntityEditorCoordinator.Delegate mEntityEditorDelegate =
            new EntityEditorCoordinator.Delegate() {
                @Override
                public void onDelete(EntityInstance entityInstance) {
                    EntityDataManager entityDataManager =
                            EntityDataManagerFactory.getForProfile(mFragment.getProfile());
                    if (entityDataManager == null) {
                        return;
                    }
                    entityDataManager.removeEntityInstance(entityInstance.getGuid());
                }

                @Override
                public void onDone(
                        EntityInstance entityInstance,
                        int descriptionStringId,
                        int acceptButtonStringId) {
                    EntityDataManager entityDataManager =
                            EntityDataManagerFactory.getForProfile(mFragment.getProfile());
                    if (entityDataManager == null) {
                        return;
                    }
                    entityDataManager.addOrUpdateEntityInstance(
                            entityInstance,
                            descriptionStringId,
                            acceptButtonStringId,
                            () -> onLocalSaveFallback());
                }

                @Override
                public void onOpenGoogleWallet(boolean isPrivateEntity) {
                    Context context = mFragment.getContext();
                    if (context == null) {
                        return;
                    }

                    if (isPrivateEntity) {
                        GoogleWalletLauncher.openGoogleWalletPrivatePassHelpCenterPage(context);
                    } else {
                        GoogleWalletLauncher.openGoogleWallet(context, context.getPackageManager());
                    }
                }
            };

    /**
     * @param fragment The fragment hosting the settings.
     * @param entityObserver Observer to be notified if entities change.
     * @param toggleConfig Information to display optIn toggle for screen, if present.
     */
    AutofillAiDelegate(
            ChromeBaseSettingsFragment fragment,
            EntityDataManager.EntityDataManagerObserver entityObserver,
            @Nullable ToggleConfig toggleConfig) {
        mFragment = fragment;
        mEntityObserver = entityObserver;
        mToggleConfig = toggleConfig;
    }

    EntityEditorCoordinator.Delegate getEntityEditorDelegate() {
        return mEntityEditorDelegate;
    }

    void onActivityCreated() {
        EntityDataManager entityDataManager =
                EntityDataManagerFactory.getForProfile(mFragment.getProfile());
        if (entityDataManager != null) {
            entityDataManager.registerDataObserver(mEntityObserver);
        }

        setupPreferenceObservers();
    }

    void onDestroyView() {
        EntityDataManager entityDataManager =
                EntityDataManagerFactory.getForProfile(mFragment.getProfile());
        if (entityDataManager != null) {
            entityDataManager.unregisterDataObserver(mEntityObserver);
        }
        if (mReauthenticatorBridge != null) {
            mReauthenticatorBridge.destroy();
            mReauthenticatorBridge = null;
        }
        destroyPreferenceObservers();
    }

    public void onConfigurationChanged() {
        if (mEntityEditor != null) {
            mEntityEditor.onConfigurationChanged();
        }
    }

    private void setupPreferenceObservers() {
        if (mToggleConfig == null) {
            return;
        }

        mPrefChangeRegistrar = PrefServiceUtil.createFor(mFragment.getProfile());
        mPrefChangeRegistrar.addObserver(
                Pref.AUTOFILL_PROFILE_ENABLED, mEntityObserver::onEntityInstancesChanged);
        mPrefChangeRegistrar.addObserver(
                mToggleConfig.prefName,
                () -> {
                    // We use postTask as if we rebuild directly during the same screen UI
                    // preference update. The change is not fully propagated through the system.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT, mEntityObserver::onEntityInstancesChanged);
                });
    }

    private void destroyPreferenceObservers() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
    }

    private static boolean shouldShowOptInToggle() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID);
    }

    private void maybeAddOptInToggle(PreferenceScreen screen, @Nullable Set<Integer> types) {
        if (!shouldShowOptInToggle() || mToggleConfig == null || types == null || types.isEmpty()) {
            return;
        }

        ChromeSwitchPreference optInToggle = new ChromeSwitchPreference(getStyledContext());
        optInToggle.setKey(mToggleConfig.key);
        optInToggle.setTitle(mToggleConfig.labelRes);
        optInToggle.setSummary(mToggleConfig.subLabelRes);

        // We compute the value and don't use directly the underlying preference.
        optInToggle.setPersistent(false);

        @EntityTypeName
        int entityType = types.iterator().next(); // We can take any of the Entity types.
        AutofillAiToggleState toggleState = getToggleState(entityType);

        optInToggle.setChecked(toggleState.checked);
        optInToggle.setEnabled(toggleState.enabled);

        optInToggle.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(mFragment.getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return toggleState.controlledByEnterprisePolicy;
                    }

                    @Override
                    public boolean isPreferenceClickDisabled(Preference preference) {
                        return !toggleState.enabled;
                    }
                });

        optInToggle.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    UserPrefs.get(mFragment.getProfile())
                            .setBoolean(mToggleConfig.prefName, (boolean) newValue);
                    return true;
                });

        screen.addPreference(optInToggle);
    }

    static void maybeAddOptInToggle(
            SettingsIndexData indexData, String prefFragmentName, ToggleConfig toggleConfig) {
        if (shouldShowOptInToggle()) {
            indexData.addEntryForKey(
                    prefFragmentName,
                    toggleConfig.key,
                    toggleConfig.labelRes,
                    toggleConfig.subLabelRes);
        }
    }

    static boolean disabledSettingsInThirdPartyMode(Profile profile) {
        return AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(
                        UserPrefs.get(profile))
                == AndroidAutofillAvailabilityStatus.AVAILABLE;
    }

    private static boolean shouldShowWalletDataSharingDataCard(Profile profile) {
        EntityDataManager entityDataManager = EntityDataManagerFactory.getForProfile(profile);
        return !disabledSettingsInThirdPartyMode(profile)
                && entityDataManager != null
                && !entityDataManager.isWalletPublicPassStorageEnabled()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER)
                && canShowWalletDataSharingPromotion(entityDataManager);
    }

    private static boolean canShowWalletDataSharingPromotion(EntityDataManager entityDataManager) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            return true;
        }

        return entityDataManager.canShowWalletDataSharingPromotion();
    }

    /** Adds an information card if sharing data from Wallet is disabled. */
    void maybeAddDisabledWalletDataSharingDataCard(PreferenceScreen screen) {
        if (shouldShowWalletDataSharingDataCard(mFragment.getProfile())) {
            addDisabledWalletDataSharingDataCard(screen);
        }
    }

    private void addDisabledWalletDataSharingDataCard(PreferenceScreen screen) {
        // LINT.IfChange(DisabledWalletDataSharingDataCard)
        CardWithButtonPreference disabledSharingWalletDataPref =
                new CardWithButtonPreference(getStyledContext(), null);
        disabledSharingWalletDataPref.setKey(DISABLED_WALLET_DATA_SHARING);
        disabledSharingWalletDataPref.setTitle(R.string.autofill_wallet_data_sharing_promo_title);
        disabledSharingWalletDataPref.setSummary(
                R.string.autofill_wallet_data_sharing_promo_subtitle);
        // LINT.ThenChange(:DynamicDisabledWalletDataSharingDataCard)
        disabledSharingWalletDataPref.setButtonText(
                mFragment
                        .getResources()
                        .getString(R.string.autofill_wallet_data_sharing_promo_button_label));
        disabledSharingWalletDataPref.setOnButtonClick(
                () -> {
                    Context context = mFragment.getContext();
                    if (context != null) {
                        GoogleWalletLauncher.openGoogleWalletPassesSettings(
                                context, context.getPackageManager());
                    }
                });

        screen.addPreference(disabledSharingWalletDataPref);
    }

    /** Adds an information card if Chrome settings are disabled in third-party mode. */
    void maybeAddDisabledSettingsInfoCard(
            PreferenceScreen screen, @AutofillOptionsReferrer int referrer) {
        if (disabledSettingsInThirdPartyMode(mFragment.getProfile())) {
            addDisabledSettingsInfoCard(screen, referrer);
        }
    }

    private void addDisabledSettingsInfoCard(
            PreferenceScreen screen, @AutofillOptionsReferrer int referrer) {
        // LINT.IfChange(AddDisabledSettingsInfoCard)
        CardWithButtonPreference disabledSettingsInfoPref =
                new CardWithButtonPreference(getStyledContext(), null);
        disabledSettingsInfoPref.setKey(DISABLED_SETTINGS_INFO);
        disabledSettingsInfoPref.setTitle(R.string.autofill_disable_settings_explanation_title);
        disabledSettingsInfoPref.setSummary(getDisabledSettingsSummaryResId());
        // LINT.ThenChange(:DynamicDisabledSettingsInfoCard)
        disabledSettingsInfoPref.setButtonText(
                mFragment
                        .getResources()
                        .getString(R.string.autofill_disable_settings_button_label));
        disabledSettingsInfoPref.setIconResource(R.drawable.ic_google_services_24dp);
        disabledSettingsInfoPref.setOnButtonClick(
                () -> {
                    SettingsNavigation settingsNavigation =
                            SettingsNavigationFactory.createSettingsNavigation();
                    settingsNavigation.startSettings(
                            getStyledContext(),
                            AutofillOptionsFragment.class,
                            AutofillOptionsFragment.createRequiredArgs(referrer),
                            /* addToBackStack= */ true);
                });

        screen.addPreference(disabledSettingsInfoPref);
    }

    private static int getDisabledSettingsSummaryResId() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
                ? R.string.autofill_disable_settings_explanation_v2
                : R.string.autofill_disable_settings_explanation;
    }

    /** Adds an information card to the search index if Chrome settings are disabled. */
    static void maybeAddDisabledSettingsInfoCard(
            SettingsIndexData indexData, Profile profile, String prefFragmentName) {
        if (disabledSettingsInThirdPartyMode(profile)) {
            if (indexData.getEntryForKey(prefFragmentName, DISABLED_SETTINGS_INFO) == null) {
                addDisabledSettingsInfoCard(indexData, prefFragmentName);
            }
        } else {
            indexData.removeEntryForKey(prefFragmentName, DISABLED_SETTINGS_INFO);
        }
    }

    private static void addDisabledSettingsInfoCard(
            SettingsIndexData indexData, String prefFragmentName) {
        // LINT.IfChange(DynamicDisabledSettingsInfoCard)
        indexData.addEntryForKey(
                prefFragmentName,
                DISABLED_SETTINGS_INFO,
                R.string.autofill_disable_settings_explanation_title,
                getDisabledSettingsSummaryResId());
        // LINT.ThenChange(:AddDisabledSettingsInfoCard)
    }

    /** Adds an information card to the search index if sharing data from Wallet is disabled. */
    static void maybeAddDisabledWalletDataSharingDataCard(
            SettingsIndexData indexData, Profile profile, String prefFragmentName) {
        if (shouldShowWalletDataSharingDataCard(profile)) {
            if (indexData.getEntryForKey(prefFragmentName, DISABLED_WALLET_DATA_SHARING) == null) {
                addDisabledWalletDataSharingDataCard(indexData, prefFragmentName);
            }
        } else {
            indexData.removeEntryForKey(prefFragmentName, DISABLED_WALLET_DATA_SHARING);
        }
    }

    private static void addDisabledWalletDataSharingDataCard(
            SettingsIndexData indexData, String prefFragmentName) {
        // LINT.IfChange(DynamicDisabledWalletDataSharingDataCard)
        indexData.addEntryForKey(
                prefFragmentName,
                DISABLED_WALLET_DATA_SHARING,
                R.string.autofill_wallet_data_sharing_promo_title,
                R.string.autofill_wallet_data_sharing_promo_subtitle);
        // LINT.ThenChange(:DisabledWalletDataSharingDataCard)
    }

    void addAutofillAiEntities(PreferenceScreen screen, @Nullable Set<Integer> typeFilter) {
        EntityDataManager entityDataManager =
                EntityDataManagerFactory.getForProfile(mFragment.getProfile());
        if (entityDataManager == null) {
            return;
        }
        if (!entityDataManager.canListEntityInstancesInSettings()
                // Autofill AI leaf pages show the UI with everything disabled in case the user
                // cannot list entities. One of Autofill and passwords goals is visibility of what
                // user can autofill.
                && !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            return;
        }

        maybeAddOptInToggle(screen, typeFilter);

        Map<EntityType, List<EntityInstanceWithLabels>> instancesToList =
                entityDataManager.getInstancesToList();

        for (Map.Entry<EntityType, List<EntityInstanceWithLabels>> entry :
                instancesToList.entrySet()) {
            EntityType type = entry.getKey();
            if (typeFilter != null && !typeFilter.contains(type.getTypeName())) {
                continue;
            }

            List<EntityInstanceWithLabels> entities = entry.getValue();

            boolean isEnabled = type.isEnabled();
            boolean isReadOnly = type.isReadOnly();
            boolean shouldHaveAddButton = isEnabled && !isReadOnly;
            if (entities.isEmpty() && !shouldHaveAddButton) {
                continue;
            }

            PreferenceCategory category = new PreferenceCategory(getStyledContext());
            category.setTitle(type.getTypeNameSectionTitleString());
            category.setKey(type.getTypeNameAsString());
            screen.addPreference(category);

            for (EntityInstanceWithLabels entity : entities) {
                Preference pref = new Preference(getStyledContext());
                pref.setTitle(entity.getEntityInstanceLabel());
                pref.setSummary(entity.getEntityInstanceSubLabel());
                pref.setKey(entity.getGuid());
                if (entity.isStoredInWallet()) {
                    pref.setWidgetLayoutResource(R.layout.google_wallet_widget);
                }
                pref.setOnPreferenceClickListener(
                        preference -> {
                            Context context = mFragment.getContext();
                            if (context == null) {
                                return true;
                            }
                            if (entity.isStoredInWallet()) {
                                String walletEntityUrl = entity.getWalletEntityUrl();
                                if (ChromeFeatureList.isEnabled(
                                                ChromeFeatureList
                                                        .AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
                                        && walletEntityUrl != null) {
                                    GoogleWalletLauncher.openGoogleWalletWithFallbackUrl(
                                            context, context.getPackageManager(), walletEntityUrl);
                                } else {
                                    GoogleWalletLauncher.openGoogleWallet(
                                            context, context.getPackageManager());
                                }
                            } else {
                                EntityInstance entityInstance =
                                        entityDataManager.getEntityInstance(preference.getKey());
                                if (entityInstance == null) {
                                    return true;
                                }
                                editEntity(entityInstance);
                            }
                            return true;
                        });
                category.addPreference(pref);
            }

            if (shouldHaveAddButton) {
                category.addPreference(createAddEntityButton(entityDataManager, type));
            }
        }
    }

    private Preference createAddEntityButton(
            EntityDataManager entityDataManager, EntityType entityType) {
        boolean buttonEnabled = isAddButtonEnabled(entityDataManager, entityType);

        Preference pref = new Preference(getStyledContext());
        Drawable plusIcon =
                ApiCompatibilityUtils.getDrawable(mFragment.getResources(), R.drawable.plus);
        plusIcon.mutate();
        plusIcon.setColorFilter(
                buttonEnabled
                        ? SemanticColorUtils.getDefaultControlColorActive(mFragment.getContext())
                        : SemanticColorUtils.getDefaultIconColorSecondary(mFragment.getContext()),
                PorterDuff.Mode.SRC_IN);
        pref.setIcon(plusIcon);
        pref.setTitle(entityType.getAddEntityTypeString());
        pref.setKey(entityType.getTypeNameAsString() + " Add"); // For testing.
        pref.setEnabled(buttonEnabled);
        pref.setOnPreferenceClickListener(
                preference -> {
                    long currentDate = TimeUtils.currentTimeMillis();
                    showEntityEditor(
                            new EntityInstance.Builder(entityType)
                                    .setModifiedDate(currentDate)
                                    .setUseCount(0)
                                    .setUseDate(currentDate)
                                    .setRecordType(
                                            entityType.isEligibleForWalletStorage()
                                                    ? RecordType.SERVER_WALLET
                                                    : RecordType.LOCAL)
                                    .setIsMaskedServerEntity(entityType.isMaskedStorageSupported())
                                    .build());
                    return true;
                });
        return pref;
    }

    private boolean isAddButtonEnabled(EntityDataManager entityDataManager, EntityType entityType) {
        return isEligibleToAddEntities(entityDataManager, entityType.getTypeName())
                && !disabledSettingsInThirdPartyMode(mFragment.getProfile());
    }

    private boolean isEligibleToAddEntities(
            EntityDataManager entityDataManager, @EntityTypeName int entityTypeName) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            return getToggleState(entityTypeName).checked;
        }

        return (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
                ? entityDataManager.canEnableOrDisableAutofillAi()
                : entityDataManager.isEligibleToAutofillAi()
                        && entityDataManager.getAutofillAiOptInStatus());
    }

    private static boolean entityTypeToggleEnabled(
            EntityDataManager entityDataManager, @EntityTypeName int entityTypeName) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)) {
            return entityDataManager.canEnableOrDisableAutofillAiForType(entityTypeName);
        } else {
            return entityDataManager.isEligibleToAutofillAiForType(entityTypeName)
                    && entityDataManager.getAutofillAiOptInStatus();
        }
    }

    private AutofillAiToggleState getToggleState(@EntityTypeName int entityTypeName) {
        EntityDataManager entityDataManager =
                EntityDataManagerFactory.getForProfile(mFragment.getProfile());
        if (entityDataManager == null || disabledSettingsInThirdPartyMode(mFragment.getProfile())) {
            return AutofillAiToggleState.DISABLED;
        }

        if (entityDataManager.getIsAutofillAiDisabledByEnterprisePolicy()) {
            return AutofillAiToggleState.DISABLED_BY_POLICY;
        }

        boolean enabled = entityTypeToggleEnabled(entityDataManager, entityTypeName);
        if (enabled) {
            if (isTogglePrefOn()) {
                return AutofillAiToggleState.ENABLED_ON;
            } else {
                return AutofillAiToggleState.ENABLED_OFF;
            }
        } else {
            return AutofillAiToggleState.DISABLED;
        }
    }

    private boolean isTogglePrefOn() {
        if (mToggleConfig == null) {
            return true;
        }

        return UserPrefs.get(mFragment.getProfile()).getBoolean(mToggleConfig.prefName);
    }

    private void editEntity(EntityInstance entityInstance) {
        if (entityInstance.requiresReauthToSee()) {
            if (mReauthenticatorBridge == null) {
                mReauthenticatorBridge =
                        ReauthenticatorBridge.create(
                                mFragment.getActivity(),
                                mFragment.getProfile(),
                                DeviceAuthSource.AUTOFILL);
            }
            if (mReauthenticatorBridge.getBiometricAvailabilityStatus()
                    != BiometricStatus.UNAVAILABLE) {
                mReauthenticatorBridge.reauthenticate(
                        success -> {
                            if (success) {
                                showEntityEditor(entityInstance);
                            }
                        });
            } else {
                showEntityEditor(entityInstance);
            }
        } else {
            showEntityEditor(entityInstance);
        }
    }

    void showEntityEditor(EntityInstance entityInstance) {
        mEntityEditor =
                new EntityEditorCoordinator(
                        mFragment.getActivity(),
                        mEntityEditorDelegate,
                        mFragment.getProfile(),
                        entityInstance);
        mEntityEditor.showEditorDialog();
    }

    private Context getStyledContext() {
        return mFragment.getPreferenceManager().getContext();
    }

    private void onLocalSaveFallback() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_AI_SHOW_DIALOG_IN_SETTINGS_WHEN_UPSTREAMING_FAILS)) {
            showConfirmationDialog();
        } else {
            showConfirmationSnackbar();
        }
    }

    private void showConfirmationDialog() {
        Activity activity = mFragment.getActivity();
        if (!(activity instanceof ModalDialogManagerHolder)) {
            return;
        }

        ModalDialogManager modalDialogManager =
                ((ModalDialogManagerHolder) activity).getModalDialogManager();
        ActionConfirmationDialog dialog =
                new ActionConfirmationDialog(activity, modalDialogManager);
        final String title =
                activity.getString(
                        R.string.autofill_ai_save_or_update_entity_failed_wallet_save_dialog_title);
        final String googleWallet = activity.getString(R.string.autofill_google_wallet_title);
        final String description =
                activity.getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_description)
                        .replace("$1", googleWallet);
        final String buttonText =
                activity.getString(
                        R.string
                                .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_confirmation_button_label);

        dialog.show(
                new ConfirmationDialogParams.Builder(activity)
                        .withTitle(title)
                        .withDescription(description)
                        .withPositiveButton(buttonText)
                        .build(),
                (dismissHandler, buttonClickResult, stopShowing) ->
                        DialogDismissType.DISMISS_IMMEDIATELY);
    }

    private void showConfirmationSnackbar() {
        if (!(mFragment.getActivity() instanceof SnackbarManager.SnackbarManageable manageable)) {
            return;
        }

        @Nullable SnackbarManager snackbarManager = manageable.getSnackbarManager();
        if (snackbarManager == null) {
            return;
        }

        final String snackbarMessage =
                mFragment
                        .getActivity()
                        .getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_title);
        Snackbar snackBar =
                Snackbar.make(
                        snackbarMessage,
                        /* controller= */ null,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_AUTOFILL_AI_LOCAL_SAVE_FALLBACK);
        final String snackbarButton =
                mFragment
                        .getActivity()
                        .getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_confirmation_button_label);
        snackBar.setAction(snackbarButton, /* actionData= */ null);
        // Wrap the message text if it doesn't fit on a single line. The action text will not wrap
        // though.
        snackBar.setDefaultLines(false);
        snackBar.setDuration(DEFAULT_SNACKBAR_DURATION);
        snackbarManager.showSnackbar(snackBar);
    }
}
