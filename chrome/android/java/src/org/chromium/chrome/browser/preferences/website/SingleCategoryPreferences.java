// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import static org.chromium.chrome.browser.preferences.SearchUtils.handleSearchNavigation;

import android.content.DialogInterface;
import android.content.res.Resources;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.app.AlertDialog;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.format.Formatter;
import android.text.style.ForegroundColorSpan;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.media.cdm.MediaDrmCredentialManager;
import org.chromium.chrome.browser.media.cdm.MediaDrmCredentialManager.MediaDrmCredentialManagerCallback;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ExpandablePreferenceGroup;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.ProtectedContentResetCredentialConfirmDialogFragment;
import org.chromium.chrome.browser.preferences.SearchUtils;
import org.chromium.chrome.browser.preferences.website.Website.StoredDataClearedCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Shows a list of sites in a particular Site Settings category. For example, this could show all
 * the websites with microphone permissions. When the user selects a site, SingleWebsitePreferences
 * is launched to allow the user to see or modify the settings for that particular website.
 */
public class SingleCategoryPreferences extends PreferenceFragment
        implements OnPreferenceChangeListener, OnPreferenceClickListener,
                   AddExceptionPreference.SiteAddedCallback,
                   ProtectedContentResetCredentialConfirmDialogFragment.Listener,
                   View.OnClickListener {
    // The key to use to pass which category this preference should display,
    // e.g. Location/Popups/All sites (if blank).
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";

    // The view to show when the list is empty.
    private TextView mEmptyView;
    // The item for searching the list of items.
    private MenuItem mSearchItem;
    // The clear button displayed in the Storage view.
    private Button mClearButton;
    // The Site Settings Category we are showing.
    private SiteSettingsCategory mCategory;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch;
    // Whether to group by allowed/blocked list.
    private boolean mGroupByAllowBlock;
    // Whether the Blocked list should be shown expanded.
    private boolean mBlockListExpanded;
    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;
    // Whether this is the first time this screen is shown.
    private boolean mIsInitialRun = true;
    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;
    // The websites that are currently displayed to the user.
    private List<WebsitePreference> mWebsites;
    // Whether tri-state ContentSetting is required.
    private boolean mRequiresTriStateSetting;

    // Keys for common ContentSetting toggle for categories. These two toggles are mutually
    // exclusive: a category should only show one of them, at most.
    public static final String BINARY_TOGGLE_KEY = "binary_toggle";
    public static final String TRI_STATE_TOGGLE_KEY = "tri_state_toggle";

    // Keys for category-specific preferences (toggle, link, button etc.), dynamically shown.
    public static final String THIRD_PARTY_COOKIES_TOGGLE_KEY = "third_party_cookies";
    public static final String NOTIFICATIONS_VIBRATE_TOGGLE_KEY = "notifications_vibrate";
    public static final String EXPLAIN_PROTECTED_MEDIA_KEY = "protected_content_learn_more";
    private static final String ADD_EXCEPTION_KEY = "add_exception";

    // Keys for Allowed/Blocked preference groups/headers.
    private static final String ALLOWED_GROUP = "allowed_group";
    private static final String BLOCKED_GROUP = "blocked_group";

    private void getInfoForOrigins() {
        if (!mCategory.enabledInAndroid(getActivity())) {
            // No need to fetch any data if we're not going to show it, but we do need to update
            // the global toggle to reflect updates in Android settings (e.g. Location).
            resetList();
            return;
        }

        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(false);
        fetcher.fetchPreferencesForCategory(mCategory, new ResultsPopulator());
    }

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;
            mWebsites = null;

            resetList();

            boolean hasEntries = mCategory.showSites(SiteSettingsCategory.Type.USB)
                    ? addChosenObjects(sites)
                    : addWebsites(sites);

            if (!hasEntries && mEmptyView != null)
                mEmptyView.setText(R.string.no_saved_website_settings);
        }
    }

    /**
     * Returns whether a website is on the Blocked list for the category currently showing.
     * @param website The website to check.
     */
    private boolean isOnBlockList(WebsitePreference website) {
        for (@SiteSettingsCategory.Type int i = 0; i < SiteSettingsCategory.Type.NUM_ENTRIES; i++) {
            if (!mCategory.showSites(i)) continue;
            for (@ContentSettingException.Type int j = 0;
                    j < ContentSettingException.Type.NUM_ENTRIES; j++) {
                if (ContentSettingException.getContentSettingsType(j)
                        == SiteSettingsCategory.contentSettingsType(i)) {
                    return ContentSetting.BLOCK == website.site().getContentSettingPermission(j);
                }
            }
            for (@PermissionInfo.Type int j = 0; j < PermissionInfo.Type.NUM_ENTRIES; j++) {
                if (PermissionInfo.getContentSettingsType(j)
                        == SiteSettingsCategory.contentSettingsType(i)) {
                    return (j == PermissionInfo.Type.MIDI)
                            ? false
                            : ContentSetting.BLOCK == website.site().getPermission(j);
                }
            }
        }
        return false;
    }

    /**
     * Update the Category Header for the Allowed list.
     * @param numAllowed The number of sites that are on the Allowed list
     * @param toggleValue The value the global toggle will have once precessing ends.
     */
    private void updateAllowedHeader(int numAllowed, boolean toggleValue) {
        ExpandablePreferenceGroup allowedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(ALLOWED_GROUP);
        if (numAllowed == 0) {
            if (allowedGroup != null) getPreferenceScreen().removePreference(allowedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // When the toggle is set to Blocked, the Allowed list header should read 'Exceptions', not
        // 'Allowed' (because it shows exceptions from the rule).
        int resourceId = toggleValue
                ? R.string.website_settings_allowed_group_heading
                : R.string.website_settings_exceptions_group_heading;
        allowedGroup.setTitle(getHeaderTitle(resourceId, numAllowed));
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void updateBlockedHeader(int numBlocked) {
        ExpandablePreferenceGroup blockedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(BLOCKED_GROUP);
        if (numBlocked == 0) {
            if (blockedGroup != null) getPreferenceScreen().removePreference(blockedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId = mCategory.showSites(SiteSettingsCategory.Type.SOUND)
                ? R.string.website_settings_blocked_group_heading_sound
                : R.string.website_settings_blocked_group_heading;
        blockedGroup.setTitle(getHeaderTitle(resourceId, numBlocked));
        blockedGroup.setExpanded(mBlockListExpanded);
    }

    private CharSequence getHeaderTitle(int resourceId, int count) {
        SpannableStringBuilder spannable =
                new SpannableStringBuilder(getResources().getString(resourceId));
        String prefCount = String.format(Locale.getDefault(), " - %d", count);
        spannable.append(prefCount);

        // Color the first part of the title blue.
        ForegroundColorSpan blueSpan = new ForegroundColorSpan(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color_link));
        spannable.setSpan(blueSpan, 0, spannable.length() - prefCount.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out the total count of items.
        int gray = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_text_color_secondary);
        spannable.setSpan(new ForegroundColorSpan(gray), spannable.length() - prefCount.length(),
                spannable.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannable;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Read which category we should be showing.
        if (getArguments() != null) {
            mCategory = SiteSettingsCategory.createFromPreferenceKey(
                    getArguments().getString(EXTRA_CATEGORY, ""));
        }
        if (mCategory == null) {
            mCategory = SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.ALL_SITES);
        }

        int contentType = mCategory.getContentSettingsType();
        mRequiresTriStateSetting =
                PrefServiceBridge.getInstance().requiresTriStateContentSetting(contentType);

        if (!mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            return super.onCreateView(inflater, container, savedInstanceState);
        } else {
            return inflater.inflate(R.layout.storage_preferences, container, false);
        }
    }

    /**
     * Returns the category being displayed. For testing.
     */
    public SiteSettingsCategory getCategoryForTest() {
        return mCategory;
    }

    /**
     * This clears all the storage for websites that are displayed to the user. This happens
     * asynchronously, and then we call {@link #getInfoForOrigins()} when we're done.
     */
    public void clearStorage() {
        if (mWebsites == null) return;
        RecordUserAction.record("MobileSettingsStorageClearAll");

        // The goal is to refresh the info for origins again after we've cleared all of them, so we
        // wait until the last website is cleared to refresh the origin list.
        final int[] numLeft = new int[1];
        numLeft[0] = mWebsites.size();
        for (int i = 0; i < mWebsites.size(); i++) {
            WebsitePreference preference = mWebsites.get(i);
            preference.site().clearAllStoredData(new StoredDataClearedCallback() {
                @Override
                public void onStoredDataCleared() {
                    if (--numLeft[0] <= 0) getInfoForOrigins();
                }
            });
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.website_preferences);
        ListView listView = (ListView) getView().findViewById(android.R.id.list);
        mEmptyView = (TextView) getView().findViewById(android.R.id.empty);
        listView.setEmptyView(mEmptyView);
        listView.setDivider(null);

        mClearButton = (Button) getView().findViewById(R.id.clear_button);
        if (mClearButton != null) mClearButton.setOnClickListener(this);

        String title = getArguments().getString(EXTRA_TITLE);
        if (title != null) getActivity().setTitle(title);

        configureGlobalToggles();

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.website_preferences_menu, menu);

        mSearchItem = menu.findItem(R.id.search);
        SearchUtils.initializeSearchView(mSearchItem, mSearch, getActivity(), (query) -> {
            boolean queryHasChanged =
                    mSearch == null ? query != null && !query.isEmpty() : !mSearch.equals(query);
            mSearch = query;
            if (queryHasChanged) getInfoForOrigins();
        });

        if (mCategory.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
            // Add a menu item to reset protected media identifier device credentials.
            MenuItem resetMenu =
                    menu.add(Menu.NONE, Menu.NONE, Menu.NONE, R.string.reset_device_credentials);
            resetMenu.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem menuItem) {
                    ProtectedContentResetCredentialConfirmDialogFragment
                            .newInstance(SingleCategoryPreferences.this)
                            .show(getFragmentManager(), null);
                    return true;
                }
            });
        }

        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            int helpContextResId = R.string.help_context_settings;
            if (mCategory.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
                helpContextResId = R.string.help_context_protected_content;
            }
            HelpAndFeedback.getInstance(getActivity()).show(
                    getActivity(), getString(helpContextResId), Profile.getLastUsedProfile(), null);
            return true;
        }
        if (handleSearchNavigation(item, mSearchItem, mSearch, getActivity())) {
            boolean queryHasChanged = mSearch != null && !mSearch.isEmpty();
            mSearch = null;
            if (queryHasChanged) getInfoForOrigins();
            return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen screen, Preference preference) {
        // Do not show the toast if the System Location setting is disabled.
        if (getPreferenceScreen().findPreference(BINARY_TOGGLE_KEY) != null
                && mCategory.isManaged()) {
            showManagedToast();
            return false;
        }

        if (preference instanceof WebsitePreference) {
            WebsitePreference website = (WebsitePreference) preference;
            website.setFragment(SingleWebsitePreferences.class.getName());
            // EXTRA_SITE re-uses already-fetched permissions, which we can only use if the Website
            // was populated with data for all permission types.
            if (mCategory.showSites(SiteSettingsCategory.Type.ALL_SITES)) {
                website.putSiteIntoExtras(SingleWebsitePreferences.EXTRA_SITE);
            } else {
                website.putSiteAddressIntoExtras(SingleWebsitePreferences.EXTRA_SITE_ADDRESS);
            }
        }

        return super.onPreferenceTreeClick(screen, preference);
    }

    /** OnClickListener for the clear button. We show an alert dialog to confirm the action */
    @Override
    public void onClick(View v) {
        if (getActivity() == null || v != mClearButton) return;

        long totalUsage = 0;
        if (mWebsites != null) {
            for (WebsitePreference preference : mWebsites) {
                totalUsage += preference.site().getTotalUsage();
            }
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder.setPositiveButton(R.string.storage_clear_dialog_clear_storage_option,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        clearStorage();
                    }
                });
        builder.setNegativeButton(R.string.cancel, null);
        builder.setTitle(R.string.storage_clear_site_storage_title);
        Resources res = getResources();
        String dialogFormattedText = res.getString(R.string.storage_clear_dialog_text,
                Formatter.formatShortFileSize(getActivity(), totalUsage));
        builder.setMessage(dialogFormattedText);
        builder.create().show();
    }

    // OnPreferenceChangeListener:
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        if (BINARY_TOGGLE_KEY.equals(preference.getKey())) {
            assert !mCategory.isManaged();

            for (@SiteSettingsCategory.Type int type = 0;
                    type < SiteSettingsCategory.Type.NUM_ENTRIES; type++) {
                if (type == SiteSettingsCategory.Type.ALL_SITES
                        || type == SiteSettingsCategory.Type.USE_STORAGE
                        || !mCategory.showSites(type)) {
                    continue;
                }

                prefServiceBridge.setCategoryEnabled(
                        SiteSettingsCategory.contentSettingsType(type), (boolean) newValue);

                if (type == SiteSettingsCategory.Type.COOKIES) {
                    updateThirdPartyCookiesCheckBox();
                } else if (type == SiteSettingsCategory.Type.NOTIFICATIONS) {
                    updateNotificationsVibrateCheckBox();
                }
                break;
            }

            // Categories that support adding exceptions also manage the 'Add site' preference.
            // This should only be used for settings that have host-pattern based exceptions.
            if (mCategory.showSites(SiteSettingsCategory.Type.AUTOPLAY)
                    || mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)
                    || mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)
                    || mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
                if ((boolean) newValue) {
                    Preference addException = getPreferenceScreen().findPreference(
                            ADD_EXCEPTION_KEY);
                    if (addException != null) {  // Can be null in testing.
                        getPreferenceScreen().removePreference(addException);
                    }
                } else {
                    getPreferenceScreen().addPreference(
                            new AddExceptionPreference(getActivity(), ADD_EXCEPTION_KEY,
                                    getAddExceptionDialogMessage(), this));
                }
            }

            ChromeSwitchPreference binaryToggle =
                    (ChromeSwitchPreference) getPreferenceScreen().findPreference(
                            BINARY_TOGGLE_KEY);
            updateAllowedHeader(mAllowedSiteCount, !binaryToggle.isChecked());

            getInfoForOrigins();
        } else if (TRI_STATE_TOGGLE_KEY.equals(preference.getKey())) {
            ContentSetting setting = (ContentSetting) newValue;
            prefServiceBridge.setContentSetting(
                    mCategory.getContentSettingsType(), setting.toInt());
            getInfoForOrigins();
        } else if (THIRD_PARTY_COOKIES_TOGGLE_KEY.equals(preference.getKey())) {
            prefServiceBridge.setBlockThirdPartyCookiesEnabled(((boolean) newValue));
        } else if (NOTIFICATIONS_VIBRATE_TOGGLE_KEY.equals(preference.getKey())) {
            prefServiceBridge.setNotificationsVibrateEnabled((boolean) newValue);
        }
        return true;
    }

    private String getAddExceptionDialogMessage() {
        int resource = 0;
        if (mCategory.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)) {
            resource = R.string.website_settings_add_site_description_automatic_downloads;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.AUTOPLAY)) {
            resource = R.string.website_settings_add_site_description_autoplay;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)) {
            resource = R.string.website_settings_add_site_description_background_sync;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)) {
            resource = PrefServiceBridge.getInstance().isCategoryEnabled(
                               ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT)
                    ? R.string.website_settings_add_site_description_javascript_block
                    : R.string.website_settings_add_site_description_javascript_allow;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            resource = PrefServiceBridge.getInstance().isCategoryEnabled(
                               ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND)
                    ? R.string.website_settings_add_site_description_sound_block
                    : R.string.website_settings_add_site_description_sound_allow;
        }
        assert resource > 0;
        return getResources().getString(resource);
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        } else {
            mBlockListExpanded = !mBlockListExpanded;
        }
        getInfoForOrigins();
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSearch == null && mSearchItem != null) {
            SearchUtils.clearSearch(mSearchItem, getActivity());
            mSearch = null;
        }

        getInfoForOrigins();
    }

    // AddExceptionPreference.SiteAddedCallback:
    @Override
    public void onAddSite(String hostname) {
        int setting = (PrefServiceBridge.getInstance().isCategoryEnabled(
                              mCategory.getContentSettingsType()))
                ? ContentSetting.BLOCK.toInt()
                : ContentSetting.ALLOW.toInt();

        PrefServiceBridge.getInstance().nativeSetContentSettingForPattern(
                mCategory.getContentSettingsType(), hostname, setting);
        Toast.makeText(getActivity(),
                String.format(getActivity().getString(
                        R.string.website_settings_add_site_toast),
                        hostname),
                Toast.LENGTH_SHORT).show();

        getInfoForOrigins();

        if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            if (setting == ContentSetting.BLOCK.toInt()) {
                RecordUserAction.record("SoundContentSetting.MuteBy.PatternException");
            } else {
                RecordUserAction.record("SoundContentSetting.UnmuteBy.PatternException");
            }
        }
    }

    /**
     * Reset the preference screen an initialize it again.
     */
    private void resetList() {
        // This will remove the combo box at the top and all the sites listed below it.
        getPreferenceScreen().removeAll();
        // And this will add the filter preference back (combo box).
        PreferenceUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        configureGlobalToggles();

        boolean exception = false;
        if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.AUTOPLAY)
                && !PrefServiceBridge.getInstance().isCategoryEnabled(
                           ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)
                && (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SITE_SETTINGS_UI)
                           || !PrefServiceBridge.getInstance().isCategoryEnabled(
                                      ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT))) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)
                && !PrefServiceBridge.getInstance().isCategoryEnabled(
                           ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)
                && !PrefServiceBridge.getInstance().isCategoryEnabled(
                           ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS)) {
            exception = true;
        }
        if (exception) {
            getPreferenceScreen().addPreference(new AddExceptionPreference(
                    getActivity(), ADD_EXCEPTION_KEY, getAddExceptionDialogMessage(), this));
        }
    }

    private boolean addWebsites(Collection<Website> sites) {
        List<WebsitePreference> websites = new ArrayList<>();

        // Find origins matching the current search.
        for (Website site : sites) {
            if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                websites.add(new WebsitePreference(getActivity(), site, mCategory));
            }
        }

        mAllowedSiteCount = 0;

        if (websites.size() == 0) {
            updateBlockedHeader(0);
            updateAllowedHeader(0, true);
            return false;
        }

        Collections.sort(websites);
        int blocked = 0;

        if (!mGroupByAllowBlock) {
            // We're not grouping sites into Allowed/Blocked lists, so show all in order
            // (will be alphabetical).
            for (WebsitePreference website : websites) {
                getPreferenceScreen().addPreference(website);
            }
        } else {
            // Group sites into Allowed/Blocked lists.
            PreferenceGroup allowedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(ALLOWED_GROUP);
            PreferenceGroup blockedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(BLOCKED_GROUP);

            for (WebsitePreference website : websites) {
                if (isOnBlockList(website)) {
                    blockedGroup.addPreference(website);
                    blocked += 1;
                } else {
                    allowedGroup.addPreference(website);
                    mAllowedSiteCount += 1;
                }
            }

            // For the ads permission, the Allowed list should appear first. Default
            // collapsed settings should not change.
            if (mCategory.showSites(SiteSettingsCategory.Type.ADS)) {
                blockedGroup.setOrder(allowedGroup.getOrder() + 1);
            }

            // The default, when the two lists are shown for the first time, is for the
            // Blocked list to be collapsed and Allowed expanded -- because the data in
            // the Allowed list is normally more useful than the data in the Blocked
            // list. A collapsed initial Blocked list works well *except* when there's
            // nothing in the Allowed list because then there's only Blocked items to
            // show and it doesn't make sense for those items to be hidden. So, in that
            // case (and only when the list is shown for the first time) do we ignore
            // the collapsed directive. The user can still collapse and expand the
            // Blocked list at will.
            if (mIsInitialRun) {
                if (allowedGroup.getPreferenceCount() == 0) mBlockListExpanded = true;
                mIsInitialRun = false;
            }

            if (!mBlockListExpanded) blockedGroup.removeAll();
            if (!mAllowListExpanded) allowedGroup.removeAll();
        }

        mWebsites = websites;
        updateBlockedHeader(blocked);
        updateAllowedHeader(mAllowedSiteCount, !isBlocked());

        return websites.size() != 0;
    }

    private boolean addChosenObjects(Collection<Website> sites) {
        Map<String, Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>>> objects =
                new HashMap<>();

        // Find chosen objects matching the current search and collect the list of sites
        // that have permission to access each.
        for (Website site : sites) {
            for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                if (mSearch.isEmpty() || info.getName().toLowerCase().contains(mSearch)) {
                    Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry =
                            objects.get(info.getObject());
                    if (entry == null) {
                        entry = Pair.create(
                                new ArrayList<ChosenObjectInfo>(), new ArrayList<Website>());
                        objects.put(info.getObject(), entry);
                    }
                    entry.first.add(info);
                    entry.second.add(site);
                }
            }
        }

        updateBlockedHeader(0);
        updateAllowedHeader(0, true);

        for (Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry : objects.values()) {
            Preference preference = new Preference(getActivity());
            Bundle extras = preference.getExtras();
            extras.putInt(
                    ChosenObjectPreferences.EXTRA_CATEGORY, mCategory.getContentSettingsType());
            extras.putString(EXTRA_TITLE, getActivity().getTitle().toString());
            extras.putSerializable(ChosenObjectPreferences.EXTRA_OBJECT_INFOS, entry.first);
            extras.putSerializable(ChosenObjectPreferences.EXTRA_SITES, entry.second);
            preference.setIcon(
                    ContentSettingsResources.getIcon(mCategory.getContentSettingsType()));
            preference.setTitle(entry.first.get(0).getName());
            preference.setFragment(ChosenObjectPreferences.class.getCanonicalName());
            getPreferenceScreen().addPreference(preference);
        }

        return objects.size() != 0;
    }

    private boolean isBlocked() {
        if (mRequiresTriStateSetting) {
            TriStateSiteSettingsPreference triStateToggle =
                    (TriStateSiteSettingsPreference) getPreferenceScreen().findPreference(
                            TRI_STATE_TOGGLE_KEY);
            return (triStateToggle.getCheckedSetting() == ContentSetting.BLOCK);
        } else {
            ChromeSwitchPreference binaryToggle =
                    (ChromeSwitchPreference) getPreferenceScreen().findPreference(
                            BINARY_TOGGLE_KEY);
            if (binaryToggle != null) return !binaryToggle.isChecked();
        }
        return false;
    }

    private void configureGlobalToggles() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        int contentType = mCategory.getContentSettingsType();
        PreferenceScreen screen = getPreferenceScreen();

        // Find all preferencs on the current preference screen. Some preferences are
        // not needed for the current category and will be removed in the steps below.
        ChromeSwitchPreference binaryToggle =
                (ChromeSwitchPreference) screen.findPreference(BINARY_TOGGLE_KEY);
        TriStateSiteSettingsPreference triStateToggle =
                (TriStateSiteSettingsPreference) screen.findPreference(TRI_STATE_TOGGLE_KEY);
        Preference thirdPartyCookies = screen.findPreference(THIRD_PARTY_COOKIES_TOGGLE_KEY);
        Preference notificationsVibrate = screen.findPreference(NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        Preference explainProtectedMediaKey = screen.findPreference(EXPLAIN_PROTECTED_MEDIA_KEY);
        PreferenceGroup allowedGroup = (PreferenceGroup) screen.findPreference(ALLOWED_GROUP);
        PreferenceGroup blockedGroup = (PreferenceGroup) screen.findPreference(BLOCKED_GROUP);

        // For these categories, no binary, tri-state or custom toggles should be shown.
        boolean hideAllToggles = mCategory.showSites(SiteSettingsCategory.Type.ALL_SITES)
                || mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)
                || mCategory.showPermissionBlockedMessage(getActivity());

        if (hideAllToggles) {
            screen.removePreference(binaryToggle);
            screen.removePreference(triStateToggle);
            screen.removePreference(thirdPartyCookies);
            screen.removePreference(notificationsVibrate);
            screen.removePreference(explainProtectedMediaKey);
            screen.removePreference(allowedGroup);
            screen.removePreference(blockedGroup);

            if (mCategory.showPermissionBlockedMessage(getActivity())) {
                // Show the link to system settings since permission is disabled.
                ChromeBasePreference osWarning = new ChromeBasePreference(getActivity(), null);
                ChromeBasePreference osWarningExtra = new ChromeBasePreference(getActivity(), null);
                mCategory.configurePermissionIsOffPreferences(
                        osWarning, osWarningExtra, getActivity(), true);
                if (osWarning.getTitle() != null) {
                    screen.addPreference(osWarning);
                }
                if (osWarningExtra.getTitle() != null) {
                    screen.addPreference(osWarningExtra);
                }
            }

            // Since all preferences are hidden, there's nothing to do further and we can
            // simply return.
            return;
        }

        // Now show and configure the binary toggle or the tri-state toggle, but not both.
        // No early return in either case since we also need to configure the category
        // specific toggles (e.g. third-party cookie) below.
        if (mRequiresTriStateSetting) {
            screen.removePreference(binaryToggle);
            configureTriStateToggle(triStateToggle, contentType);
        } else {
            screen.removePreference(triStateToggle);
            configureBinaryToggle(binaryToggle, contentType);
        }

        // Configure/hide the third-party cookie toggle, as needed.
        if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)) {
            thirdPartyCookies.setOnPreferenceChangeListener(this);
            updateThirdPartyCookiesCheckBox();
        } else {
            screen.removePreference(thirdPartyCookies);
        }

        // Configure/hide the notifications vibrate toggle, as needed.
        if (mCategory.showSites(SiteSettingsCategory.Type.NOTIFICATIONS)
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            notificationsVibrate.setOnPreferenceChangeListener(this);
            updateNotificationsVibrateCheckBox();
        } else {
            screen.removePreference(notificationsVibrate);
        }

        // Only show the link that explains protected content settings when needed.
        if (!mCategory.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
            screen.removePreference(explainProtectedMediaKey);
        }

        // When this menu opens, make sure the Blocked list is collapsed.
        if (!mGroupByAllowBlock) {
            mBlockListExpanded = false;
            mAllowListExpanded = true;
        }
        mGroupByAllowBlock = true;

        allowedGroup.setOnPreferenceClickListener(this);
        blockedGroup.setOnPreferenceClickListener(this);
    }

    private void configureTriStateToggle(
            TriStateSiteSettingsPreference triStateToggle, int contentType) {
        triStateToggle.setOnPreferenceChangeListener(this);
        ContentSetting setting = ContentSetting.fromInt(
                PrefServiceBridge.getInstance().getContentSetting(contentType));
        int[] descriptionIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(contentType);
        triStateToggle.initialize(setting, descriptionIds);
    }

    private void configureBinaryToggle(ChromeSwitchPreference binaryToggle, int contentType) {
        binaryToggle.setOnPreferenceChangeListener(this);
        binaryToggle.setTitle(ContentSettingsResources.getTitle(contentType));

        // Set summary on or off.
        if (mCategory.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)
                && PrefServiceBridge.getInstance().isLocationAllowedByPolicy()) {
            binaryToggle.setSummaryOn(ContentSettingsResources.getGeolocationAllowedSummary());
        } else {
            binaryToggle.setSummaryOn(ContentSettingsResources.getEnabledSummary(contentType));
        }
        binaryToggle.setSummaryOff(ContentSettingsResources.getDisabledSummary(contentType));

        binaryToggle.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                // TODO(bauerb): Align the ManagedPreferenceDelegate and
                // SiteSettingsCategory interfaces better to avoid this indirection.
                return mCategory.isManaged() && !mCategory.isManagedByCustodian();
            }

            @Override
            public boolean isPreferenceControlledByCustodian(Preference preference) {
                return mCategory.isManagedByCustodian();
            }
        });

        // Set the checked value.
        if (mCategory.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)) {
            binaryToggle.setChecked(
                    LocationSettings.getInstance().isChromeLocationSettingEnabled());
        } else {
            binaryToggle.setChecked(PrefServiceBridge.getInstance().isCategoryEnabled(contentType));
        }
    }

    private void updateThirdPartyCookiesCheckBox() {
        ChromeBaseCheckBoxPreference thirdPartyCookiesPref =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        THIRD_PARTY_COOKIES_TOGGLE_KEY);
        thirdPartyCookiesPref.setChecked(
                PrefServiceBridge.getInstance().isBlockThirdPartyCookiesEnabled());
        thirdPartyCookiesPref.setEnabled(PrefServiceBridge.getInstance().isCategoryEnabled(
                ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES));
        thirdPartyCookiesPref.setManagedPreferenceDelegate(
                preference -> PrefServiceBridge.getInstance().isBlockThirdPartyCookiesManaged());
    }

    private void updateNotificationsVibrateCheckBox() {
        ChromeBaseCheckBoxPreference preference =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        if (preference != null) {
            preference.setEnabled(PrefServiceBridge.getInstance().isCategoryEnabled(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
        }
    }

    private void showManagedToast() {
        if (mCategory.isManagedByCustodian()) {
            ManagedPreferencesUtils.showManagedByParentToast(getActivity());
        } else {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity());
        }
    }

    // ProtectedContentResetCredentialConfirmDialogFragment.Listener:
    @Override
    public void resetDeviceCredential() {
        MediaDrmCredentialManager.resetCredentials(new MediaDrmCredentialManagerCallback() {
            @Override
            public void onCredentialResetFinished(boolean succeeded) {
                if (succeeded) return;
                Toast.makeText(getActivity(), getString(R.string.protected_content_reset_failed),
                             Toast.LENGTH_SHORT)
                        .show();
            }
        });
    }
}
