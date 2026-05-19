// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Fragment for listing and managing permissions for Glic actor login. */
@NullMarked
public class GlicActorLoginPermissionsFragment extends ChromeBaseSettingsFragment {

    private static final String CATEGORY_KEY = "actor_login_permissions_category";
    private static final String DESCRIPTION_KEY = "actor_login_permissions_description";
    private static final String EMPTY_KEY = "actor_login_permissions_empty";
    private static final String MANAGED_KEY = "actor_login_permissions_managed";

    private PreferenceCategory mCategory;
    private Preference mDescriptionPref;
    private CardPreference mEmptyCard;
    private CardPreference mManagedCard;
    private LargeIconBridge mLargeIconBridge;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private GlicActorLoginBridge mBridge;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.glic_actor_login_permissions);
        mPageTitle.set(getString(R.string.settings_glic_actor_login_permissions_section_title));
        mCategory = assertNonNull(findPreference(CATEGORY_KEY));
        mDescriptionPref = assertNonNull(mCategory.findPreference(DESCRIPTION_KEY));

        mEmptyCard = assertNonNull(mCategory.findPreference(EMPTY_KEY));
        mEmptyCard.setSummary(getString(R.string.settings_glic_login_permissions_no_sites));
        mEmptyCard.setIconDrawable(
                AppCompatResources.getDrawable(getContext(), R.drawable.ic_language_24));
        mEmptyCard.setShouldCenterIcon(true);
        mEmptyCard.setCloseIconVisibility(View.GONE);

        mManagedCard = assertNonNull(mCategory.findPreference(MANAGED_KEY));
        mManagedCard.setSummary(getString(R.string.managed_by_your_organization));
        mManagedCard.setIconDrawable(
                AppCompatResources.getDrawable(getContext(), R.drawable.ic_domain));
        mManagedCard.setShouldCenterIcon(true);
        mManagedCard.setCloseIconVisibility(View.GONE);

        mLargeIconBridge = new LargeIconBridge(getProfile());

        mBridge = new GlicActorLoginBridge(getProfile());
        mBridge.getAllPermissions(this::populatePermissions);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mLargeIconBridge.destroy();
        mBridge.destroy();
    }

    @VisibleForTesting
    void populatePermissions(List<ActorLoginPermission> permissions) {
        mCategory.removeAll();

        mDescriptionPref.setOrder(0);
        mCategory.addPreference(mDescriptionPref);

        boolean isOffline = !NetworkChangeNotifier.isOnline();

        int order = 1;
        if (!isOffline) {
            for (ActorLoginPermission permission : permissions) {
                ActorLoginPermissionPreference pref =
                        new ActorLoginPermissionPreference(
                                getContext(),
                                permission,
                                mLargeIconBridge,
                                p -> onRevokeClicked(p, permission));
                pref.setOrder(order++);
                mCategory.addPreference(pref);
            }
        }

        mEmptyCard.setOrder(order++);
        mManagedCard.setOrder(order);

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isManaged =
                prefService.isManagedPreference(GlicPrefNames.GLIC_ACTUATION_ON_WEB)
                        || prefService.isManagedPreference(
                                GlicPrefNames.GLIC_ACTUATION_ON_WEB_ALLOWED_FOR_UR_LS)
                        || prefService.isManagedPreference(
                                GlicPrefNames.GLIC_ACTUATION_ON_WEB_BLOCKED_FOR_UR_LS);

        if (isOffline) {
            mEmptyCard.setSummary(
                    getString(R.string.settings_glic_login_permissions_offline_warning));
            Drawable offlineIcon =
                    AppCompatResources.getDrawable(getContext(), R.drawable.ic_error);
            if (offlineIcon != null) {
                offlineIcon.setTint(SemanticColorUtils.getDefaultIconColor(getContext()));
                mEmptyCard.setIconDrawable(offlineIcon);
            }
            mEmptyCard.setVisible(true);
            mCategory.addPreference(mEmptyCard);
        } else if (permissions.isEmpty()) {
            mEmptyCard.setSummary(getString(R.string.settings_glic_login_permissions_no_sites));
            mEmptyCard.setIconDrawable(
                    AppCompatResources.getDrawable(getContext(), R.drawable.ic_language_24));
            mEmptyCard.setVisible(true);
            mCategory.addPreference(mEmptyCard);
        } else {
            mEmptyCard.setVisible(false);
        }

        if (isManaged) {
            mManagedCard.setVisible(true);
            mCategory.addPreference(mManagedCard);
        } else {
            mManagedCard.setVisible(false);
        }

        notifyPreferencesUpdated();
    }

    @VisibleForTesting
    void onRevokeClicked(ActorLoginPermissionPreference pref, ActorLoginPermission permission) {
        showRevokeConfirmationDialog(pref, permission);
    }

    private void showRevokeConfirmationDialog(
            ActorLoginPermissionPreference pref, ActorLoginPermission permission) {
        ModalDialogManagerHolder holder = assertNonNull((ModalDialogManagerHolder) getActivity());
        ModalDialogManager modalDialogManager = holder.getModalDialogManager();
        if (modalDialogManager == null) return;

        SimpleModalDialogController dialogController =
                new SimpleModalDialogController(
                        modalDialogManager,
                        dismissalCause -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                revokePermission(pref, permission);
                            }
                        });

        PropertyModel dialog =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                getString(R.string.settings_glic_revoke_actor_login_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getString(
                                        R.string
                                                .settings_glic_revoke_actor_login_dialog_description))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                getString(R.string.remove))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                getString(R.string.cancel))
                        .build();
        modalDialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.APP);
    }

    private void revokePermission(
            ActorLoginPermissionPreference pref, ActorLoginPermission permission) {
        mBridge.revokePermission(
                permission.getSignonRealm(),
                permission.getUsername(),
                success -> {
                    if (!success) return;
                    mCategory.removePreference(pref);
                    notifyPreferencesUpdated();
                });
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
