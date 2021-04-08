// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

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

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.webauthn.CableAuthenticatorModuleProvider;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.SpanApplier;

import java.util.Locale;

/**
 * The "Passwords" screen in Settings, which allows the user to enable or disable password saving,
 * to view saved passwords (just the username and URL), and to delete saved passwords.
 */
public class PasswordSettings
        extends PreferenceFragmentCompat implements PasswordManagerHandler.PasswordListObserver,
                                                    Preference.OnPreferenceClickListener {
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
    public static final String PREF_KEY_MANAGE_ACCOUNT_LINK = "manage_account_link";
    public static final String PREF_KEY_SECURITY_KEY_LINK = "security_key_link";

    // A PasswordEntryViewer receives a boolean value with this key. If set true, the the entry was

    // part of a search result.
    public static final String EXTRA_FOUND_VIA_SEARCH = "found_via_search_args";

    private static final String PREF_KEY_CATEGORY_SAVED_PASSWORDS = "saved_passwords";
    private static final String PREF_KEY_CATEGORY_EXCEPTIONS = "exceptions";
    private static final String PREF_KEY_SAVED_PASSWORDS_NO_TEXT = "saved_passwords_no_text";

    private static final int ORDER_SWITCH = 0;
    private static final int ORDER_AUTO_SIGNIN_CHECKBOX = 1;
    private static final int ORDER_CHECK_PASSWORDS = 2;
    private static final int ORDER_MANAGE_ACCOUNT_LINK = 3;
    private static final int ORDER_SECURITY_KEY = 4;
    private static final int ORDER_SAVED_PASSWORDS = 5;
    private static final int ORDER_EXCEPTIONS = 6;
    private static final int ORDER_SAVED_PASSWORDS_NO_TEXT = 7;

    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;

    private MenuItem mHelpItem;
    private MenuItem mSearchItem;

    private String mSearchQuery;
    private Preference mLinkPref;
    private Preference mSecurityKey;
    private ChromeSwitchPreference mSavePasswordsSwitch;
    private ChromeSwitchPreference mAutoSignInSwitch;
    private ChromeBasePreference mCheckPasswords;
    private TextMessagePreference mEmptyView;
    private boolean mSearchRecorded;
    private Menu mMenu;

    private @Nullable PasswordCheck mPasswordCheck;
    private @ManagePasswordsReferrer int mManagePasswordsReferrer;

    /**
     * For controlling the UX flow of exporting passwords.
     */
    private ExportFlow mExportFlow = new ExportFlow();

    public ExportFlow getExportFlowForTesting() {
        return mExportFlow;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mExportFlow.onCreate(savedInstanceState, new ExportFlow.Delegate() {
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
        });
        getActivity().setTitle(R.string.password_settings_title);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        PasswordManagerHandlerProvider.getInstance().addObserver(this);

        setHasOptionsMenu(true); // Password Export might be optional but Search is always present.

        mManagePasswordsReferrer = getReferrerFromInstanceStateOrLaunchBundle(savedInstanceState);

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(SAVED_STATE_SEARCH_QUERY)) {
            mSearchQuery = savedInstanceState.getString(SAVED_STATE_SEARCH_QUERY);
            mSearchRecorded = mSearchQuery != null; // We record a search when a query is set.
        }
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
        mPasswordCheck = PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl());
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
            mExportFlow.startExporting();
            return true;
        }
        if (SearchUtils.handleSearchNavigation(item, mSearchItem, mSearchQuery, getActivity())) {
            filterPasswords(null);
            return true;
        }
        if (id == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_passwords), Profile.getLastUsedRegularProfile(),
                    null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void filterPasswords(String query) {
        mSearchQuery = query;
        mHelpItem.setShowAsAction(mSearchQuery == null ? MenuItem.SHOW_AS_ACTION_IF_ROOM
                                                       : MenuItem.SHOW_AS_ACTION_NEVER);
        rebuildPasswordLists();
    }

    /**
     * Empty screen message when no passwords or exceptions are stored.
     */
    private void displayEmptyScreenMessage() {
        mEmptyView = new TextMessagePreference(getStyledContext(), null);
        mEmptyView.setSummary(R.string.saved_passwords_none_text);
        mEmptyView.setKey(PREF_KEY_SAVED_PASSWORDS_NO_TEXT);
        mEmptyView.setOrder(ORDER_SAVED_PASSWORDS_NO_TEXT);
        mEmptyView.setDividerAllowedAbove(false);
        mEmptyView.setDividerAllowedBelow(false);
        getPreferenceScreen().addPreference(mEmptyView);
    }

    /**
     * Include a message when there's no match.
     */
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
        if (mSearchQuery == null) {
            createSavePasswordsSwitch();
            createAutoSignInCheckbox();
            if (mPasswordCheck != null) {
                createCheckPasswords();
            }
        }
        PasswordManagerHandlerProvider.getInstance()
                .getPasswordManagerHandler()
                .updatePasswordLists();
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

    /**
     * Removes the message informing the user that there are no saved entries to display.
     */
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

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_AUTH_PHONE_SUPPORT)) {
            displaySecurityKeyLink();
        }

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
            profileCategory.setTitle(R.string.password_settings_title);
            profileCategory.setOrder(ORDER_SAVED_PASSWORDS);
            getPreferenceScreen().addPreference(profileCategory);
            passwordParent = profileCategory;
        } else {
            passwordParent = getPreferenceScreen();
        }
        for (int i = 0; i < count; i++) {
            SavedPasswordEntry saved = PasswordManagerHandlerProvider.getInstance()
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
                getView().announceForAccessibility(
                        getString(R.string.accessible_find_in_page_no_results));
            }
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
            String exception = PasswordManagerHandlerProvider.getInstance()
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
        PasswordManagerHandlerProvider.getInstance().removeObserver(this);
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing() && mPasswordCheck != null
                && mManagePasswordsReferrer != ManagePasswordsReferrer.CHROME_SETTINGS) {
            PasswordCheckFactory.destroy();
        }
    }

    /**
     *  Preference was clicked. Either navigate to manage account site or launch the PasswordEditor
     *  depending on which preference it was.
     */
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mLinkPref) {
            Intent intent = new Intent(
                    Intent.ACTION_VIEW, Uri.parse(PasswordUIView.getAccountDashboardURL()));
            intent.setPackage(getActivity().getPackageName());
            getActivity().startActivity(intent);
        } else if (ChromeFeatureList.isEnabled(ChromeFeatureList.EDIT_PASSWORDS_IN_SETTINGS)) {
            boolean isBlockedCredential =
                    !preference.getExtras().containsKey(PasswordSettings.PASSWORD_LIST_NAME);
            PasswordManagerHandlerProvider.getInstance()
                    .getPasswordManagerHandler()
                    .showPasswordEntryEditingView(getActivity(), new SettingsLauncherImpl(),
                            preference.getExtras().getInt(PasswordSettings.PASSWORD_LIST_ID),
                            isBlockedCredential);
        } else {
            // Launch preference activity with PasswordEntryViewer fragment with
            // intent extras specifying the object.
            Bundle fragmentAgs = new Bundle(preference.getExtras());
            fragmentAgs.putBoolean(PasswordSettings.EXTRA_FOUND_VIA_SEARCH, mSearchQuery != null);
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(
                    getActivity(), PasswordEntryViewer.class, fragmentAgs);
        }
        return true;
    }

    private void createSavePasswordsSwitch() {
        mSavePasswordsSwitch = new ChromeSwitchPreference(getStyledContext(), null);
        mSavePasswordsSwitch.setKey(PREF_SAVE_PASSWORDS_SWITCH);
        mSavePasswordsSwitch.setTitle(R.string.password_settings_save_passwords);
        mSavePasswordsSwitch.setOrder(ORDER_SWITCH);
        mSavePasswordsSwitch.setSummaryOn(R.string.text_on);
        mSavePasswordsSwitch.setSummaryOff(R.string.text_off);
        mSavePasswordsSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, (boolean) newValue);
            return true;
        });
        mSavePasswordsSwitch.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE));

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            getPreferenceScreen().addPreference(mSavePasswordsSwitch);
        }

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when save passwords is really turned off), so
        // .setChecked() should be called after .addPreference()
        mSavePasswordsSwitch.setChecked(
                getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
    }

    private void createAutoSignInCheckbox() {
        mAutoSignInSwitch = new ChromeSwitchPreference(getStyledContext(), null);
        mAutoSignInSwitch.setKey(PREF_AUTOSIGNIN_SWITCH);
        mAutoSignInSwitch.setTitle(R.string.passwords_auto_signin_title);
        mAutoSignInSwitch.setOrder(ORDER_AUTO_SIGNIN_CHECKBOX);
        mAutoSignInSwitch.setSummary(R.string.passwords_auto_signin_description);
        mAutoSignInSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, (boolean) newValue);
            return true;
        });
        mAutoSignInSwitch.setManagedPreferenceDelegate((ChromeManagedPreferenceDelegate) preference
                -> getPrefService().isManagedPreference(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
        getPreferenceScreen().addPreference(mAutoSignInSwitch);
        mAutoSignInSwitch.setChecked(
                getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
    }

    private void createCheckPasswords() {
        mCheckPasswords = new ChromeBasePreference(getStyledContext());
        mCheckPasswords.setKey(PREF_CHECK_PASSWORDS);
        mCheckPasswords.setTitle(R.string.passwords_check_title);
        mCheckPasswords.setOrder(ORDER_CHECK_PASSWORDS);
        mCheckPasswords.setSummary(R.string.passwords_check_description);
        // Add a listener which launches a settings page for the leak password check
        mCheckPasswords.setOnPreferenceClickListener(preference -> {
            PasswordCheck passwordCheck =
                    PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl());
            passwordCheck.showUi(getStyledContext(), PasswordCheckReferrer.PASSWORD_SETTINGS);
            // Return true to notify the click was handled
            return true;
        });
        getPreferenceScreen().addPreference(mCheckPasswords);
    }

    private void displayManageAccountLink() {
        if (!PasswordManagerLauncher.isSyncingPasswordsWithoutCustomPassphrase()) {
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
        ForegroundColorSpan colorSpan = new ForegroundColorSpan(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color_link));
        SpannableString title = SpanApplier.applySpans(getString(R.string.manage_passwords_text),
                new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
        mLinkPref = new ChromeBasePreference(getStyledContext());
        mLinkPref.setKey(PREF_KEY_MANAGE_ACCOUNT_LINK);
        mLinkPref.setTitle(title);
        mLinkPref.setOnPreferenceClickListener(this);
        mLinkPref.setOrder(ORDER_MANAGE_ACCOUNT_LINK);
        getPreferenceScreen().addPreference(mLinkPref);
    }

    private void displaySecurityKeyLink() {
        if (mSecurityKey == null) {
            mSecurityKey = new ChromeBasePreference(getStyledContext());
            mSecurityKey.setKey(PREF_KEY_SECURITY_KEY_LINK);
            mSecurityKey.setTitle(R.string.phone_as_security_key_text);
            mSecurityKey.setOnPreferenceClickListener(preference -> {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(
                        getActivity(), CableAuthenticatorModuleProvider.class, null);
                return true;
            });
            mSecurityKey.setOrder(ORDER_SECURITY_KEY);
        }
        getPreferenceScreen().addPreference(mSecurityKey);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    Menu getMenuForTesting() {
        return mMenu;
    }

    @VisibleForTesting
    Toolbar getToolbarForTesting() {
        return getActivity().findViewById(R.id.action_bar);
    }
}
