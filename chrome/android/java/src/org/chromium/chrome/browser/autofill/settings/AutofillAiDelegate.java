// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.user_prefs.UserPrefs;

import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneId;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** A delegate class to handle shared logic for Forms AI settings fragments. */
@NullMarked
public class AutofillAiDelegate {
    private static final int DEFAULT_SNACKBAR_DURATION = 10000;

    private final ChromeBaseSettingsFragment mFragment;
    private final EntityDataManager.EntityDataManagerObserver mEntityObserver;
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
                    entityDataManager.removeEntityInstance(entityInstance.getGUID());
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
     */
    AutofillAiDelegate(ChromeBaseSettingsFragment fragment, EntityDataManager.EntityDataManagerObserver entityObserver) {
        mFragment = fragment;
        mEntityObserver = entityObserver;
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
    }

    public void onConfigurationChanged() {
        if (mEntityEditor != null) {
            mEntityEditor.onConfigurationChanged();
        }
    }

    static boolean disabledSettingsInThirdPartyMode(Profile profile) {
        return AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(
                        UserPrefs.get(profile))
                == AndroidAutofillAvailabilityStatus.AVAILABLE;
    }

    void addAutofillAiEntities(PreferenceScreen screen, @Nullable Set<Integer> typeFilter) {
        EntityDataManager entityDataManager =
                EntityDataManagerFactory.getForProfile(mFragment.getProfile());
        if (entityDataManager == null) {
            return;
        }
        if (!entityDataManager.canListEntityInstancesInSettings()) {
            return;
        }

        Map<EntityType, List<EntityInstanceWithLabels>> instancesToList =
                entityDataManager.getInstancesToList();

        boolean isEligibleToAddEntities =
                (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
                        ? entityDataManager.canEnableOrDisableAutofillAi()
                        : entityDataManager.isEligibleToAutofillAi()
                                && entityDataManager.getAutofillAiOptInStatus());
        boolean addButtonEnabled =
                isEligibleToAddEntities
                        && !disabledSettingsInThirdPartyMode(mFragment.getProfile());

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
            category.setTitle(type.getTypeNameAsString());
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
                category.addPreference(createAddEntityButton(type, !addButtonEnabled));
            }
        }
    }

    private Preference createAddEntityButton(EntityType entityType, boolean disabled) {
        Preference pref = new Preference(getStyledContext());
        Drawable plusIcon =
                ApiCompatibilityUtils.getDrawable(mFragment.getResources(), R.drawable.plus);
        plusIcon.mutate();
        plusIcon.setColorFilter(
                disabled
                        ? SemanticColorUtils.getDefaultIconColorSecondary(mFragment.getContext())
                        : SemanticColorUtils.getDefaultControlColorActive(mFragment.getContext()),
                PorterDuff.Mode.SRC_IN);
        pref.setIcon(plusIcon);
        pref.setTitle(entityType.getAddEntityTypeString());
        pref.setKey(entityType.getTypeNameAsString() + " Add"); // For testing.
        pref.setEnabled(!disabled);
        pref.setOnPreferenceClickListener(
                preference -> {
                    Instant nowInstant = Instant.ofEpochMilli(TimeUtils.currentTimeMillis());
                    LocalDate modifiedDate =
                            nowInstant.atZone(ZoneId.systemDefault()).toLocalDate();
                    showEntityEditor(
                            new EntityInstance.Builder(entityType)
                                    .setModifiedDate(modifiedDate)
                                    .setUseCount(0)
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
