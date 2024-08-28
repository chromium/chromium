// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PASSWORD_SETTINGS_EXPORT_METRICS_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.SpanApplier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * The "Passwords" screen in Settings, which allows the user to enable or disable password saving,
 * to view saved passwords (just the username and URL), and to delete saved passwords.
 */
public class PasswordSettings extends ChromeBaseSettingsFragment
        implements PasswordListObserver,
                Preference.OnPreferenceClickListener,
                SyncService.SyncStateChangedListener {
    @IntDef({
        TrustedVaultBannerState.NOT_SHOWN,
        TrustedVaultBannerState.OFFER_OPT_IN,
        TrustedVaultBannerState.OPTED_IN
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TrustedVaultBannerState {
        int NOT_SHOWN = 0;
        int OFFER_OPT_IN = 1;
        int OPTED_IN = 2;
    }

    // Keys for name/password dictionaries.
    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    // The key for saving |mSearchQuery| to instance bundle.
    private static final String SAVED_STATE_SEARCH_QUERY = "saved-state-search-query";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";
    public static final String PREF_CHECK_PASSWORDS = "check_passwords";
    public static final String PREF_TRUSTED_VAULT_BANNER = "trusted_vault_banner";
    public static final String PREF_KEY_MANAGE_ACCOUNT_LINK = "manage_account_link";

    private static final String PREF_KEY_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_KEY_CATEGORY_EXCEPTIONS = "exceptions";
    private static final String PREF_KEY_SAVED_PASSWORDS_NO_TEXT = "saved_passwords_no_text";

    private static final int ORDER_SWITCH = 0;
    private static final int ORDER_AUTO_SIGNIN_CHECKBOX = 1;
    private static final int ORDER_CHECK_PASSWORDS = 2;
    private static final int ORDER_TRUSTED_VAULT_BANNER = 3;
    private static final int ORDER_MANAGE_ACCOUNT_LINK = 4;
    private static final int ORDER_SAVED_PASSWORDS = 6;
    private static final int ORDER_EXCEPTIONS = 7;
    private static final int ORDER_SAVED_PASSWORDS_NO_TEXT = 8;

    // This request code is not actually consumed today in onActivityResult() but is defined here to
    // avoid bugs in the future if the request code is reused.
    private static final int REQUEST_CODE_TRUSTED_VAULT_OPT_IN = 1;

    // Unique request code for the password exporting activity.
    private static final int PASSWORD_EXPORT_INTENT_REQUEST_CODE = 3485764;

    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;
    private @TrustedVaultBannerState int mTrustedVaultBannerState =
            TrustedVaultBannerState.NOT_SHOWN;

    private MenuItem mHelpItem;
    private MenuItem mSearchItem;

    private String mSearchQuery;
    private Preference mLinkPref;
    private Menu mMenu;

    private @Nullable PasswordCheck mPasswordCheck;
    private @ManagePasswordsReferrer int mManagePasswordsReferrer;
    private OneshotSupplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /** For controlling the UX flow of exporting passwords. */
    private ExportFlow mExportFlow = new ExportFlow();

    public ExportFlow getExportFlowForTesting() {
        return mExportFlow;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mExportFlow.onCreate(
                savedInstanceState,
                new ExportFlow.Delegate() {
                    @Override
                    public Activity getActivity() {
                        return PasswordSettings.this.getActivity();
                    }

                    @Override
                    public FragmentManager getFragmentManager() {
                        return PasswordSettings.this.getFragmentManager();
                    }

                    @Override
                    public int getViewId() {
                        return getView().getId();
                    }

                    @Override
                    public void runCreateFileOnDiskIntent(Intent intent) {
                        startActivityForResult(intent, PASSWORD_EXPORT_INTENT_REQUEST_CODE);
                    }

                    @Override
                    public Profile getProfile() {
                        return PasswordSettings.this.getProfile();
                    }
                },
                PASSWORD_SETTINGS_EXPORT_METRICS_ID);
        mPageTitle.set(getString(R.string.password_manager_settings_title));
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        PasswordManagerHandlerProvider.getForProfile(getProfile()).addObserver(this);

        if (SyncServiceFactory.getForProfile(getProfile()) != null) {
            SyncServiceFactory.getForProfile(getProfile()).addSyncStateChangedListener(this);
        }

        setHasOptionsMenu(true); // Password Export might be optional but Search is always present.

        mManagePasswordsReferrer = getReferrerFromInstanceStateOrLaunchBundle(savedInstanceState);

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(SAVED_STATE_SEARCH_QUERY)) {
            mSearchQuery = savedInstanceState.getString(SAVED_STATE_SEARCH_QUERY);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private @ManagePasswordsReferrer int getReferrerFromInstanceStateOrLaunchBundle(
            Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(
                        PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER)) {
            return savedInstanceState.getInt(PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER);
        }
        Bundle extras = getArguments();
        assert extras.containsKey(PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER)
                : "PasswordSettings must be launched with a manage-passwords-referrer fragment"
                        + "argument, but none was provided.";
        return extras.getInt(PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPasswordCheck = PasswordCheckFactory.getOrCreate();
        computeTrustedVaultBannerState();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        mMenu = menu;
        inflater.inflate(R.menu.save_password_preferences_action_bar_menu, menu);
        menu.findItem(R.id.export_passwords).setVisible(ExportFlow.providesPasswordExport());
        menu.findItem(R.id.export_passwords).setEnabled(false);
        mSearchItem = menu.findItem(R.id.menu_id_search);
        mSearchItem.setVisible(true);
        mHelpItem = menu.findItem(R.id.menu_id_targeted_help);
        SearchUtils.initializeSearchView(
                mSearchItem, mSearchQuery, getActivity(), this::filterPasswords);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        menu.findItem(R.id.export_passwords).setEnabled(!mNoPasswords && !mExportFlow.isActive());
        super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.export_passwords) {
            RecordHistogram.recordEnumeratedHistogram(
                    mExportFlow.getExportEventHistogramName(),
                    ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED,
                    ExportFlow.PasswordExportEvent.COUNT);
            mExportFlow.startExporting();
            return true;
        }
        if (SearchUtils.handleSearchNavigation(item, mSearchItem, mSearchQuery, getActivity())) {
            filterPasswords(null);
            return true;
        }
        if (id == R.id.menu_id_targeted_help) {
            getHelpAndFeedbackLauncher()
                    .show(getActivity(), getString(R.string.help_context_passwords), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void filterPasswords(String query) {
        mSearchQuery = query;
        mHelpItem.setShowAsAction(
                mSearchQuery == null
                        ? MenuItem.SHOW_AS_ACTION_IF_ROOM
                        : MenuItem.SHOW_AS_ACTION_NEVER);
        rebuildPasswordLists();
    }

    /** Empty screen message when no passwords or exceptions are stored. */
    private void displayEmptyScreenMessage() {
        TextMessagePreference emptyView = new TextMessagePreference(getStyledContext(), null);
        emptyView.setSummary(R.string.saved_passwords_none_text);
        emptyView.setKey(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        emptyView.setOrder(ORDER_SAVED_PASSWORDS_NO_TEXT);
        emptyView.setDividerAllowedAbove(false);
        emptyView.setDividerAllowedBelow(false);
        getPreferenceScreen().addPreference(emptyView);
    }

    /** Include a message when there's no match. */
    private void displayPasswordNoResultScreenMessage() {
        Preference noResultView = new Preference(getStyledContext());
        noResultView.setLayoutResource(R.layout.password_no_result);
        noResultView.setSelectable(false);
        getPreferenceScreen().addPreference(noResultView);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        ReauthenticationManager.resetLastReauth();
    }

    void rebuildPasswordLists() {
        mNoPasswords = false;
        mNoPasswordExceptions = false;
        getPreferenceScreen().removeAll();
        if (mSearchQuery != null) {
            // Only the filtered passwords and exceptions should be shown.
            PasswordManagerHandlerProvider.getForProfile(getProfile())
                    .getPasswordManagerHandler()
                    .updatePasswordLists();
            return;
        }

        createSavePasswordsSwitch();
        if (shouldShowAutoSigninOption()) {
            createAutoSignInCheckbox();
        }
        if (mPasswordCheck != null) {
            createCheckPasswords();
        }

        if (mTrustedVaultBannerState == TrustedVaultBannerState.OPTED_IN) {
            createTrustedVaultBanner(
                    R.string.android_trusted_vault_banner_sub_label_opted_in,
                    this::openTrustedVaultInfoPage);
        } else if (mTrustedVaultBannerState == TrustedVaultBannerState.OFFER_OPT_IN) {
            createTrustedVaultBanner(
                    R.string.android_trusted_vault_banner_sub_label_offer_opt_in,
                    this::openTrustedVaultOptInDialog);
        }
        PasswordManagerHandlerProvider.getForProfile(getProfile())
                .getPasswordManagerHandler()
                .updatePasswordLists();
    }

    private boolean shouldShowAutoSigninOption() {
        return !BuildInfo.getInstance().isAutomotive;
    }

    /**
     * Removes the UI displaying the list of saved passwords or exceptions.
     * @param preferenceCategoryKey The key string identifying the PreferenceCategory to be removed.
     */
    private void resetList(String preferenceCategoryKey) {
        PreferenceCategory profileCategory =
                (PreferenceCategory) getPreferenceScreen().findPreference(preferenceCategoryKey);
        if (profileCategory != null) {
            profileCategory.removeAll();
            getPreferenceScreen().removePreference(profileCategory);
        }
    }

    /** Removes the message informing the user that there are no saved entries to display. */
    private void resetNoEntriesTextMessage() {
        Preference message = getPreferenceScreen().findPreference(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        if (message != null) {
            getPreferenceScreen().removePreference(message);
        }
    }

    @Override
    public void passwordListAvailable(int count) {
        resetList(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
        resetNoEntriesTextMessage();

        mNoPasswords = count == 0;
        if (mNoPasswords) {
            if (mNoPasswordExceptions) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceGroup passwordParent;
        if (mSearchQuery == null) {
            PreferenceCategory profileCategory = new PreferenceCategory(getStyledContext());
            profileCategory.setKey(PREF_KEY_CATEGORY_SAVED_PASSWORDS);
            profileCategory.setTitle(R.string.password_list_title);
            profileCategory.setOrder(ORDER_SAVED_PASSWORDS);
            getPreferenceScreen().addPreference(profileCategory);
            passwordParent = profileCategory;
        } else {
            passwordParent = getPreferenceScreen();
        }
        for (int i = 0; i < count; i++) {
            SavedPasswordEntry saved =
                    PasswordManagerHandlerProvider.getForProfile(getProfile())
                            .getPasswordManagerHandler()
                            .getSavedPasswordEntry(i);
            String url = saved.getUrl();
            String name = saved.getUserName();
            String password = saved.getPassword();
            if (shouldBeFiltered(url, name)) {
                continue; // The current password won't show with the active filter, try the next.
            }
            Preference preference = new Preference(getStyledContext());
            preference.setTitle(url);
            preference.setOnPreferenceClickListener(this);
            preference.setSummary(name);
            Bundle args = preference.getExtras();
            args.putString(PASSWORD_LIST_NAME, name);
            args.putString(PASSWORD_LIST_URL, url);
            args.putString(PASSWORD_LIST_PASSWORD, password);
            args.putInt(PASSWORD_LIST_ID, i);
            passwordParent.addPreference(preference);
        }
        mNoPasswords = passwordParent.getPreferenceCount() == 0;
        if (mMenu != null) {
            mMenu.findItem(R.id.export_passwords)
                    .setEnabled(!mNoPasswords && !mExportFlow.isActive());
        }
        if (mNoPasswords) {
            if (count == 0) displayEmptyScreenMessage(); // Show if the list was already empty.
            if (mSearchQuery == null) {
                // If not searching, the category needs to be removed again.
                getPreferenceScreen().removePreference(passwordParent);
            } else {
                displayPasswordNoResultScreenMessage();
                getView()
                        .announceForAccessibility(
                                getString(R.string.accessible_find_in_page_no_results));
            }
        }

        if (!mNoPasswords) {
            PasswordManagerHandlerProvider.getForProfile(getProfile())
                    .getPasswordManagerHandler()
                    .showMigrationWarning(getActivity(), mBottomSheetControllerSupplier.get());
        }
    }

    /**
     * Returns true if there is a search query that requires the exclusion of an entry based on
     * the passed url or name.
     * @param url the visible URL of the entry to check. May be empty but must not be null.
     * @param name the visible user name of the entry to check. May be empty but must not be null.
     * @return Returns whether the entry with the passed url and name should be filtered.
     */
    private boolean shouldBeFiltered(final String url, final String name) {
        if (mSearchQuery == null) {
            return false;
        }
        return !url.toLowerCase(Locale.ENGLISH).contains(mSearchQuery.toLowerCase(Locale.ENGLISH))
                && !name.toLowerCase(Locale.getDefault())
                        .contains(mSearchQuery.toLowerCase(Locale.getDefault()));
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        if (mSearchQuery != null) return; // Don't show exceptions if a search is ongoing.
        resetList(PREF_KEY_CATEGORY_EXCEPTIONS);
        resetNoEntriesTextMessage();

        mNoPasswordExceptions = count == 0;
        if (mNoPasswordExceptions) {
            if (mNoPasswords) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        PreferenceCategory profileCategory = new PreferenceCategory(getStyledContext());
        profileCategory.setKey(PREF_KEY_CATEGORY_EXCEPTIONS);
        profileCategory.setTitle(R.string.section_saved_passwords_exceptions);
        profileCategory.setOrder(ORDER_EXCEPTIONS);
        getPreferenceScreen().addPreference(profileCategory);
        for (int i = 0; i < count; i++) {
            String exception =
                    PasswordManagerHandlerProvider.getForProfile(getProfile())
                            .getPasswordManagerHandler()
                            .getSavedPasswordException(i);
            Preference preference = new Preference(getStyledContext());
            preference.setTitle(exception);
            preference.setOnPreferenceClickListener(this);
            Bundle args = preference.getExtras();
            args.putString(PASSWORD_LIST_URL, exception);
            args.putInt(PASSWORD_LIST_ID, i);
            profileCategory.addPreference(preference);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mExportFlow.onResume();
        rebuildPasswordLists();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        super.onActivityResult(requestCode, resultCode, intent);
        if (requestCode != PASSWORD_EXPORT_INTENT_REQUEST_CODE) return;
        if (resultCode != Activity.RESULT_OK) return;
        if (intent == null || intent.getData() == null) return;

        mExportFlow.savePasswordsToDownloads(intent.getData());
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mExportFlow.onSaveInstanceState(outState);
        if (mSearchQuery != null) {
            outState.putString(SAVED_STATE_SEARCH_QUERY, mSearchQuery);
        }
        outState.putInt(PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER, mManagePasswordsReferrer);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (SyncServiceFactory.getForProfile(getProfile()) != null) {
            SyncServiceFactory.getForProfile(getProfile()).removeSyncStateChangedListener(this);
        }
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing()) {
            PasswordManagerHandlerProvider.getForProfile(getProfile()).removeObserver(this);
            if (mPasswordCheck != null
                    && mManagePasswordsReferrer != ManagePasswordsReferrer.CHROME_SETTINGS) {
                PasswordCheckFactory.destroy();
            }
        }
    }

    /**
     *  Preference was clicked. Either navigate to manage account site or launch the PasswordEditor
     *  depending on which preference it was.
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mLinkPref) {
            Intent intent =
                    new Intent(
                            Intent.ACTION_VIEW, Uri.parse(PasswordUIView.getAccountDashboardURL()));
            intent.setPackage(getActivity().getPackageName());
            getActivity().startActivity(intent);
        } else {
            boolean isBlockedCredential =
                    !preference.getExtras().containsKey(PasswordSettings.PASSWORD_LIST_NAME);
            PasswordManagerHandlerProvider.getForProfile(getProfile())
                    .getPasswordManagerHandler()
                    .showPasswordEntryEditingView(
                            getActivity(),
                            preference.getExtras().getInt(PasswordSettings.PASSWORD_LIST_ID),
                            isBlockedCredential);
        }
        return true;
    }

    private void createSavePasswordsSwitch() {
        ChromeSwitchPreference savePasswordsSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        savePasswordsSwitch.setKey(PREF_SAVE_PASSWORDS_SWITCH);
        savePasswordsSwitch.setTitle(R.string.password_settings_save_passwords);
        savePasswordsSwitch.setOrder(ORDER_SWITCH);
        savePasswordsSwitch.setSummaryOn(R.string.text_on);
        savePasswordsSwitch.setSummaryOff(R.string.text_off);
        savePasswordsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    getPrefService()
                            .setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, (boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            "PasswordManager.Settings.ToggleOfferToSavePasswords",
                            (boolean) newValue);
                    // TODO(http://crbug.com/1371422): Remove method and manage evictions from
                    // native code as this is covered by chrome://password-manager-internals page.
                    if ((boolean) newValue) {
                        PasswordManagerHelper.getForProfile(getProfile()).resetUpmUnenrollment();
                    }
                    return true;
                });
        savePasswordsSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return getPrefService()
                                .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
                    }
                });

        getPreferenceScreen().addPreference(savePasswordsSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when save passwords is really turned off), so
        // .setChecked() should be called after .addPreference()
        savePasswordsSwitch.setChecked(
                getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
    }

    private void createAutoSignInCheckbox() {
        ChromeSwitchPreference autoSignInSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autoSignInSwitch.setKey(PREF_AUTOSIGNIN_SWITCH);
        autoSignInSwitch.setTitle(R.string.passwords_auto_signin_title);
        autoSignInSwitch.setOrder(ORDER_AUTO_SIGNIN_CHECKBOX);
        autoSignInSwitch.setSummary(R.string.passwords_auto_signin_description);
        autoSignInSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    getPrefService()
                            .setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, (boolean) newValue);
                    RecordHistogram.recordBooleanHistogram(
                            "PasswordManager.Settings.ToggleAutoSignIn", (boolean) newValue);
                    return true;
                });
        autoSignInSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return getPrefService()
                                .isManagedPreference(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN);
                    }
                });
        getPreferenceScreen().addPreference(autoSignInSwitch);
        autoSignInSwitch.setChecked(
                getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
    }

    private void createCheckPasswords() {
        ChromeBasePreference checkPasswords = new ChromeBasePreference(getStyledContext());
        checkPasswords.setKey(PREF_CHECK_PASSWORDS);
        checkPasswords.setTitle(R.string.passwords_check_title);
        checkPasswords.setOrder(ORDER_CHECK_PASSWORDS);
        checkPasswords.setSummary(R.string.passwords_check_description);
        // Add a listener which launches a settings page for the leak password check
        checkPasswords.setOnPreferenceClickListener(
                preference -> {
                    PasswordCheck passwordCheck = PasswordCheckFactory.getOrCreate();
                    passwordCheck.showUi(
                            getStyledContext(), PasswordCheckReferrer.PASSWORD_SETTINGS);
                    // Return true to notify the click was handled.
                    return true;
                });
        getPreferenceScreen().addPreference(checkPasswords);
    }

    private void createTrustedVaultBanner(
            @StringRes int subLabel, Preference.OnPreferenceClickListener listener) {
        ChromeBasePreference trustedVaultBanner = new ChromeBasePreference(getStyledContext());
        trustedVaultBanner.setKey(PREF_TRUSTED_VAULT_BANNER);
        trustedVaultBanner.setTitle(R.string.android_trusted_vault_banner_label);
        trustedVaultBanner.setOrder(ORDER_TRUSTED_VAULT_BANNER);
        trustedVaultBanner.setSummary(subLabel);
        trustedVaultBanner.setOnPreferenceClickListener(listener);
        getPreferenceScreen().addPreference(trustedVaultBanner);
    }

    private void displayManageAccountLink() {
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService == null || !syncService.isEngineInitialized()) {
            return;
        }
        if (!PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(syncService)) {
            return;
        }
        if (mSearchQuery != null && !mNoPasswords) {
            return; // Don't add the Manage Account link if there is a search going on.
        }
        if (getPreferenceScreen().findPreference(PREF_KEY_MANAGE_ACCOUNT_LINK) != null) {
            return; // Don't add the Manage Account link if it's present.
        }
        if (mLinkPref != null) {
            // If we created the link before, reuse it.
            getPreferenceScreen().addPreference(mLinkPref);
            return;
        }
        ForegroundColorSpan colorSpan =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorLink(getContext()));
        SpannableString title =
                SpanApplier.applySpans(
                        getString(R.string.manage_passwords_text),
                        new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
        mLinkPref = new ChromeBasePreference(getStyledContext());
        mLinkPref.setKey(PREF_KEY_MANAGE_ACCOUNT_LINK);
        mLinkPref.setTitle(title);
        mLinkPref.setOnPreferenceClickListener(this);
        mLinkPref.setOrder(ORDER_MANAGE_ACCOUNT_LINK);
        getPreferenceScreen().addPreference(mLinkPref);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private PrefService getPrefService() {
        return UserPrefs.get(getProfile());
    }

    @Override
    public void syncStateChanged() {
        final @TrustedVaultBannerState int oldTrustedVaultBannerState = mTrustedVaultBannerState;
        computeTrustedVaultBannerState();
        if (oldTrustedVaultBannerState != mTrustedVaultBannerState) {
            rebuildPasswordLists();
        }
    }

    private void computeTrustedVaultBannerState() {
        final SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService == null) {
            mTrustedVaultBannerState = TrustedVaultBannerState.NOT_SHOWN;
            return;
        }
        if (!syncService.isEngineInitialized()) {
            // Can't call getPassphraseType() yet.
            mTrustedVaultBannerState = TrustedVaultBannerState.NOT_SHOWN;
            return;
        }
        if (syncService.getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE) {
            mTrustedVaultBannerState = TrustedVaultBannerState.OPTED_IN;
            return;
        }
        if (syncService.shouldOfferTrustedVaultOptIn()) {
            mTrustedVaultBannerState = TrustedVaultBannerState.OFFER_OPT_IN;
            return;
        }
        mTrustedVaultBannerState = TrustedVaultBannerState.NOT_SHOWN;
    }

    private boolean openTrustedVaultOptInDialog(Preference unused) {
        assert SyncServiceFactory.getForProfile(getProfile()) != null;
        CoreAccountInfo accountInfo =
                SyncServiceFactory.getForProfile(getProfile()).getAccountInfo();
        assert accountInfo != null;
        SyncSettingsUtils.openTrustedVaultOptInDialog(
                this, accountInfo, REQUEST_CODE_TRUSTED_VAULT_OPT_IN);
        // Return true to notify the click was handled.
        return true;
    }

    private boolean openTrustedVaultInfoPage(Preference unused) {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(PasswordUIView.getTrustedVaultLearnMoreURL()));
        intent.setPackage(getActivity().getPackageName());
        getActivity().startActivity(intent);
        // Return true to notify the click was handled.
        return true;
    }

    public void setBottomSheetControllerSupplier(
            OneshotSupplier<BottomSheetController> bottomSheetControllerSupplier) {
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
    }

    Menu getMenuForTesting() {
        return mMenu;
    }

    Toolbar getToolbarForTesting() {
        return getActivity().findViewById(R.id.action_bar);
    }
}
