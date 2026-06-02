// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.password_manager.settings.PasswordsPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.signin_promo.AutofillAndPasswordsPromoDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Home of Transactions fragment, the main entry point for all Autofill and Passwords settings. */
@NullMarked
public class HomeOfTransactionsFragment extends ChromeBaseSettingsFragment {

    public static final String PREF_SIGNIN_PROMO = "autofill_and_passwords_signin_promo";
    public static final String PREF_PASSWORDS = "autofill_and_passwords_gpm";
    public static final String PREF_AUTOFILL_PAYMENTS = "autofill_and_passwords_payments";
    public static final String PREF_AUTOFILL_ADDRESSES = "autofill_and_passwords_addresses";
    public static final String PREF_AUTOFILL_IDENTITY_DOCS = "autofill_and_passwords_identity_docs";
    public static final String PREF_AUTOFILL_TRAVEL = "autofill_and_passwords_travel";
    public static final String PREF_AUTOFILL_SHOPPING = "autofill_and_passwords_shopping";
    public static final String PREF_AUTOFILL_SETTINGS = "autofill_and_passwords_settings";

    public static final String EXTRA_REFERRER = "autofill_and_passwords_referrer";

    // Represents different referrers when navigating to the Home of Transactions page.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // Needs to stay in sync with AutofillSettingsReferrer in enums.xml. Clank currently uses only
    // the SETTINGS_MENU value.
    // LINT.IfChange(AutofillSettingsReferrer)
    @IntDef({AutofillSettingsReferrer.SETTINGS_MENU, AutofillSettingsReferrer.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AutofillSettingsReferrer {

        // int PROFILE_MENU = 0,

        /** Corresponds to Chrome's main settings menu. */
        int SETTINGS_MENU = 1;

        // int AUTOFILL_AND_PASSWORDS_PAGE = 2,
        // int FILLING_FLOW_DROPDOWN = 3,

        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillSettingsReferrer)

    /**
     * These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     *
     * <p>Must be kept in sync with the YourSavedInfoDataCategory enum in
     * histograms/metadata/autofill/enums.xml
     */
    // LINT.IfChange(YourSavedInfoDataCategory)
    @IntDef({
        YourSavedInfoDataCategory.PASSWORD_MANAGER,
        YourSavedInfoDataCategory.PAYMENTS,
        YourSavedInfoDataCategory.CONTACT_INFO,
        YourSavedInfoDataCategory.IDENTITY_DOCS,
        YourSavedInfoDataCategory.TRAVEL,
        YourSavedInfoDataCategory.SHOPPING,
        YourSavedInfoDataCategory.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface YourSavedInfoDataCategory {
        int PASSWORD_MANAGER = 0;
        int PAYMENTS = 1;
        int CONTACT_INFO = 2;
        int IDENTITY_DOCS = 3;
        int TRAVEL = 4;
        int SHOPPING = 5;
        int COUNT = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:YourSavedInfoDataCategory)

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private OneshotSupplier<WindowAndroid> mWindowAndroidSupplier;
    private ActivityResultTracker mActivityResultTracker;
    private OneshotSupplier<BottomSheetController> mBottomSheetControllerSupplier;
    private OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private @Nullable SigninPromoCoordinator mSigninPromoCoordinator;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_and_passwords_settings_title));

        @AutofillSettingsReferrer
        int referrer =
                getArguments() != null
                        ? getArguments()
                                .getInt(EXTRA_REFERRER, AutofillSettingsReferrer.SETTINGS_MENU)
                        : AutofillSettingsReferrer.SETTINGS_MENU;

        if (savedInstanceState == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Autofill.YourSavedInfoSettingsPage.VisitReferrer",
                    referrer,
                    AutofillSettingsReferrer.COUNT);
        }

        requireActivity()
                .addMenuProvider(new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);

        SettingsUtils.addPreferencesFromResource(this, R.xml.home_of_transactions_preferences);

        setupSignInPromo();

        PasswordsPreference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setProfile(getProfile());
        passwordsPreference.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        passwordsPreference.setOnPreferenceClickListener(
                preference -> {
                    recordCategoryLinkClick(YourSavedInfoDataCategory.PASSWORD_MANAGER);
                    PasswordManagerLauncher.showPasswordSettings(
                            getContext(),
                            getProfile(),
                            ManagePasswordsReferrer.CHROME_SETTINGS_AUTOFILL_AND_PASSWORDS,
                            mModalDialogManagerSupplier.asNonNull().get(),
                            /* managePasskeys= */ false);
                    return true;
                });

        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(
                        preference -> {
                            recordCategoryLinkClick(YourSavedInfoDataCategory.PAYMENTS);
                            return SettingsNavigationHelper.showAutofillCreditCardSettings(
                                    getActivity(), /* addToBackStack= */ true);
                        });

        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(
                        preference -> {
                            recordCategoryLinkClick(YourSavedInfoDataCategory.CONTACT_INFO);
                            return SettingsNavigationHelper.showAutofillProfileSettings(
                                    getActivity(), /* addToBackStack= */ true);
                        });

        Preference identityDocsPref = findPreference(PREF_AUTOFILL_IDENTITY_DOCS);
        identityDocsPref.setVisible(shouldShowAutofillAiSettings());
        identityDocsPref.setOnPreferenceClickListener(
                preference -> {
                    recordCategoryLinkClick(YourSavedInfoDataCategory.IDENTITY_DOCS);
                    return SettingsNavigationHelper.showAutofillIdentityDocsSettings(getActivity());
                });

        Preference travelPref = findPreference(PREF_AUTOFILL_TRAVEL);
        travelPref.setVisible(shouldShowAutofillAiSettings());
        travelPref.setOnPreferenceClickListener(
                preference -> {
                    recordCategoryLinkClick(YourSavedInfoDataCategory.TRAVEL);
                    return SettingsNavigationHelper.showAutofillTravelSettings(getActivity());
                });

        Preference shoppingPref = findPreference(PREF_AUTOFILL_SHOPPING);
        shoppingPref.setVisible(shouldShowShopping());
        shoppingPref.setOnPreferenceClickListener(
                preference -> {
                    recordCategoryLinkClick(YourSavedInfoDataCategory.SHOPPING);
                    return SettingsNavigationHelper.showAutofillShoppingSettings(getActivity());
                });

        findPreference(PREF_AUTOFILL_SETTINGS)
                .setOnPreferenceClickListener(
                        preference -> {
                            SettingsNavigationFactory.createSettingsNavigation()
                                    .startSettings(
                                            getContext(),
                                            AutofillOptionsFragment.class,
                                            AutofillOptionsFragment.createRequiredArgs(
                                                    AutofillOptionsReferrer
                                                            .AUTOFILL_AND_PASSWORDS_FRAGMENT),
                                            /* addToBackStack= */ true);
                            return true;
                        });
    }

    @Initializer
    public void setDependencies(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            OneshotSupplier<WindowAndroid> windowAndroidSupplier,
            ActivityResultTracker activityResultTracker,
            OneshotSupplier<BottomSheetController> bottomSheetControllerSupplier,
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mWindowAndroidSupplier = windowAndroidSupplier;
        mActivityResultTracker = activityResultTracker;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    @Override
    public void onDestroy() {
        if (mSigninPromoCoordinator != null) {
            mSigninPromoCoordinator.destroy();
        }
        super.onDestroy();
    }

    private void setupSignInPromo() {
        SupplierUtils.waitForAll(
                () -> {
                    if (getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.INITIALIZED)) {
                        mSigninPromoCoordinator = createSigninPromoCoordinator();
                        ((SigninPromoPreference) findPreference(PREF_SIGNIN_PROMO))
                                .setCoordinator(mSigninPromoCoordinator);
                        updateSignInPromo();
                    }
                },
                mWindowAndroidSupplier,
                mBottomSheetControllerSupplier,
                mSnackbarManagerSupplier,
                mModalDialogManagerSupplier);
    }

    private SigninPromoCoordinator createSigninPromoCoordinator() {
        return new SigninPromoCoordinator(
                SupplierUtils.asNonNull(mWindowAndroidSupplier).get(),
                getActivity(),
                getProfile(),
                mActivityResultTracker,
                SigninAndHistorySyncActivityLauncherImpl.get(),
                SupplierUtils.asNonNull(mBottomSheetControllerSupplier),
                mModalDialogManagerSupplier.asNonNull().get(),
                SupplierUtils.asNonNull(mSnackbarManagerSupplier).get(),
                DeviceLockActivityLauncherImpl.get(),
                new AutofillAndPasswordsPromoDelegate(
                        getContext(),
                        getProfile(),
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        this::updateSignInPromo));
    }

    private void updateSignInPromo() {
        SigninPromoPreference promoPreference = findPreference(PREF_SIGNIN_PROMO);
        if (mSigninPromoCoordinator != null && mSigninPromoCoordinator.canShowPromo()) {
            promoPreference.setVisible(true);
        } else {
            promoPreference.setVisible(false);
        }
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    private static boolean shouldShowAutofillAiSettings() {
        return ChromeFeatureList.isEnabled(YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA);
    }

    private static boolean shouldShowShopping() {
        return shouldShowAutofillAiSettings()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WALLET_SHOPPING);
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return UserPrefs.get(getProfile())
                            .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    // Password manager is always enabled, even if disabled by policy. In such case
                    // a disclaimer text is displayed.
                    return false;
                }
                return super.isPreferenceClickDisabled(preference);
            }
        };
    }

    private void recordCategoryLinkClick(@YourSavedInfoDataCategory int category) {
        RecordHistogram.recordEnumeratedHistogram(
                "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
                category,
                YourSavedInfoDataCategory.COUNT);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    HomeOfTransactionsFragment.class.getName(),
                    R.xml.home_of_transactions_preferences) {

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {

                    // Always remove the sign-in promo - the SigninPromoPreference is a placeholder
                    // - we don't want it to be indexed.
                    indexData.removeEntry(getUniqueId(PREF_SIGNIN_PROMO));

                    boolean featureDisabled =
                            !ChromeFeatureList.isEnabled(YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID);
                    if (featureDisabled) {
                        indexData.removeEntry(getUniqueId(PREF_PASSWORDS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_PAYMENTS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_ADDRESSES));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SETTINGS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_IDENTITY_DOCS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_TRAVEL));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SHOPPING));
                    } else {
                        if (!shouldShowAutofillAiSettings()) {
                            indexData.removeEntry(getUniqueId(PREF_AUTOFILL_IDENTITY_DOCS));
                            indexData.removeEntry(getUniqueId(PREF_AUTOFILL_TRAVEL));
                        }
                        if (!shouldShowShopping()) {
                            indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SHOPPING));
                        }
                    }
                }
            };
}
