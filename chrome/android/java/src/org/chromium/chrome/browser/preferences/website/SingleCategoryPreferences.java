// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import static org.chromium.chrome.browser.preferences.SearchUtils.handleSearchNavigation;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Build;
import android.os.Bundle;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceGroup;
import android.support.v7.preference.PreferenceManager;
import android.support.v7.preference.PreferenceScreen;
import android.support.v7.widget.RecyclerView;
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
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ExpandablePreferenceGroup;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.SearchUtils;
import org.chromium.chrome.browser.preferences.website.Website.StoredDataClearedCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Shows a list of sites in a particular Site Settings category. For example, this could show all
 * the websites with microphone permissions. When the user selects a site, SingleWebsitePreferences
 * is launched to allow the user to see or modify the settings for that particular website.
 */
public class SingleCategoryPreferences extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener,
                   AddExceptionPreference.SiteAddedCallback, View.OnClickListener,
                   PreferenceManager.OnPreferenceTreeClickListener {
    // The key to use to pass which category this preference should display,
    // e.g. Location/Popups/All sites (if blank).
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";

    /**
     * If present, the list of websites will be filtered by domain using
     * {@link UrlUtilities#getDomainAndRegistry}.
     */
    public static final String EXTRA_SELECTED_DOMAINS = "selected_domains";

    // The list that contains preferences.
    private RecyclerView mListView;
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
    // Whether the Managed list should be shown expanded.
    private boolean mManagedListExpanded;
    // Whether this is the first time this screen is shown.
    private boolean mIsInitialRun = true;
    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;
    // The websites that are currently displayed to the user.
    private List<WebsitePreference> mWebsites;
    // Whether tri-state ContentSetting is required.
    private boolean mRequiresTriStateSetting;

    @Nullable
    private Set<String> mSelectedDomains;

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
    private static final String MANAGED_GROUP = "managed_group";

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

            int chooserDataType = mCategory.getObjectChooserDataType();
            boolean hasEntries =
                    chooserDataType == -1 ? addWebsites(sites) : addChosenObjects(sites);

            if (mEmptyView == null) return;

            mEmptyView.setVisibility(hasEntries ? View.GONE : View.VISIBLE);
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
                    return ContentSettingValues.BLOCK
                            == website.site().getContentSettingPermission(j);
                }
            }
            for (@PermissionInfo.Type int j = 0; j < PermissionInfo.Type.NUM_ENTRIES; j++) {
                if (PermissionInfo.getContentSettingsType(j)
                        == SiteSettingsCategory.contentSettingsType(i)) {
                    return (j == PermissionInfo.Type.MIDI)
                            ? false
                            : ContentSettingValues.BLOCK == website.site().getPermission(j);
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
        if (allowedGroup == null) return;

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

    private void updateManagedHeader(int numManaged) {
        ExpandablePreferenceGroup managedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(MANAGED_GROUP);
        if (numManaged == 0) {
            if (managedGroup != null) getPreferenceScreen().removePreference(managedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId = R.string.website_settings_managed_group_heading;
        managedGroup.setTitle(getHeaderTitle(resourceId, numManaged));
        managedGroup.setExpanded(mManagedListExpanded);
    }

    private CharSequence getHeaderTitle(int resourceId, int count) {
        SpannableStringBuilder spannable = new SpannableStringBuilder(getString(resourceId));
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
                WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);

        ViewGroup view = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);

        // Add custom views for Storage Preferences to bottom of the fragment.
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            inflater.inflate(R.layout.storage_preferences_view, view, true);
            mEmptyView = view.findViewById(R.id.empty_storage);
            mClearButton = view.findViewById(R.id.clear_button);
            mClearButton.setOnClickListener(this);
        }

        mListView = getListView();

        // Disable animations of preference changes.
        mListView.setItemAnimator(null);

        // Remove dividers between preferences.
        setDivider(null);

        return view;
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
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Handled in onActivityCreated. Moving the addPreferencesFromResource call up to here
        // causes animation jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        String title = getArguments().getString(EXTRA_TITLE);
        if (title != null) getActivity().setTitle(title);

        mSelectedDomains = getArguments().containsKey(EXTRA_SELECTED_DOMAINS)
                ? new HashSet<>(getArguments().getStringArrayList(EXTRA_SELECTED_DOMAINS))
                : null;

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
            HelpAndFeedback.getInstance().show(
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
    public boolean onPreferenceTreeClick(Preference preference) {
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
            int navigationSource = getArguments().getInt(
                    SettingsNavigationSource.EXTRA_KEY, SettingsNavigationSource.OTHER);
            website.getExtras().putInt(SettingsNavigationSource.EXTRA_KEY, navigationSource);
        }

        return super.onPreferenceTreeClick(preference);
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
        String dialogFormattedText = getString(R.string.storage_clear_dialog_text,
                Formatter.formatShortFileSize(getActivity(), totalUsage));
        builder.setMessage(dialogFormattedText);
        builder.create().show();
    }

    // OnPreferenceChangeListener:
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (BINARY_TOGGLE_KEY.equals(preference.getKey())) {
            assert !mCategory.isManaged();

            for (@SiteSettingsCategory.Type int type = 0;
                    type < SiteSettingsCategory.Type.NUM_ENTRIES; type++) {
                if (type == SiteSettingsCategory.Type.ALL_SITES
                        || type == SiteSettingsCategory.Type.USE_STORAGE
                        || !mCategory.showSites(type)) {
                    continue;
                }

                WebsitePreferenceBridge.setCategoryEnabled(
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
                    || (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)
                            && ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH))
                    || mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)
                    || mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
                if ((boolean) newValue) {
                    Preference addException =
                            getPreferenceScreen().findPreference(ADD_EXCEPTION_KEY);
                    if (addException != null) { // Can be null in testing.
                        getPreferenceScreen().removePreference(addException);
                    }
                } else {
                    getPreferenceScreen().addPreference(
                            new AddExceptionPreference(getStyledContext(), ADD_EXCEPTION_KEY,
                                    getAddExceptionDialogMessage(), this));
                }
            }

            ChromeSwitchPreference binaryToggle =
                    (ChromeSwitchPreference) getPreferenceScreen().findPreference(
                            BINARY_TOGGLE_KEY);
            updateAllowedHeader(mAllowedSiteCount, !binaryToggle.isChecked());

            getInfoForOrigins();
        } else if (TRI_STATE_TOGGLE_KEY.equals(preference.getKey())) {
            @ContentSettingValues
            int setting = (int) newValue;
            WebsitePreferenceBridge.setContentSetting(mCategory.getContentSettingsType(), setting);
            getInfoForOrigins();
        } else if (THIRD_PARTY_COOKIES_TOGGLE_KEY.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.BLOCK_THIRD_PARTY_COOKIES, ((boolean) newValue));
        } else if (NOTIFICATIONS_VIBRATE_TOGGLE_KEY.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.NOTIFICATIONS_VIBRATE_ENABLED, (boolean) newValue);
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
            resource = WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.JAVASCRIPT)
                    ? R.string.website_settings_add_site_description_javascript_block
                    : R.string.website_settings_add_site_description_javascript_allow;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            resource = WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.SOUND)
                    ? R.string.website_settings_add_site_description_sound_block
                    : R.string.website_settings_add_site_description_sound_allow;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)) {
            resource = WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.COOKIES)
                    ? R.string.website_settings_add_site_description_cookies_block
                    : R.string.website_settings_add_site_description_cookies_allow;
        }
        assert resource > 0;
        return getString(resource);
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        } else if (BLOCKED_GROUP.equals(preference.getKey())) {
            mBlockListExpanded = !mBlockListExpanded;
        } else {
            mManagedListExpanded = !mManagedListExpanded;
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
        int setting =
                (WebsitePreferenceBridge.isCategoryEnabled(mCategory.getContentSettingsType()))
                ? ContentSettingValues.BLOCK
                : ContentSettingValues.ALLOW;

        WebsitePreferenceBridge.setContentSettingForPattern(
                mCategory.getContentSettingsType(), hostname, setting);
        Toast.makeText(getActivity(),
                String.format(getActivity().getString(
                        R.string.website_settings_add_site_toast),
                        hostname),
                Toast.LENGTH_SHORT).show();

        getInfoForOrigins();

        if (mCategory.showSites(SiteSettingsCategory.Type.SOUND)) {
            if (setting == ContentSettingValues.BLOCK) {
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
                && !WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.AUTOPLAY)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.JAVASCRIPT)
                && (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH)
                        || !WebsitePreferenceBridge.isCategoryEnabled(
                                ContentSettingsType.JAVASCRIPT))) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.COOKIES)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        ContentSettingsType.BACKGROUND_SYNC)) {
            exception = true;
        } else if (mCategory.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        ContentSettingsType.AUTOMATIC_DOWNLOADS)) {
            exception = true;
        }
        if (exception) {
            getPreferenceScreen().addPreference(new AddExceptionPreference(
                    getStyledContext(), ADD_EXCEPTION_KEY, getAddExceptionDialogMessage(), this));
        }
    }

    private boolean addWebsites(Collection<Website> sites) {
        filterSelectedDomains(sites);

        List<WebsitePreference> websites = new ArrayList<>();

        // Find origins matching the current search.
        for (Website site : sites) {
            if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                websites.add(new WebsitePreference(getStyledContext(), site, mCategory));
            }
        }

        mAllowedSiteCount = 0;

        if (websites.size() == 0) {
            updateBlockedHeader(0);
            updateAllowedHeader(0, true);
            updateManagedHeader(0);
            return false;
        }

        Collections.sort(websites);
        int blocked = 0;
        int managed = 0;

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
            PreferenceGroup managedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(MANAGED_GROUP);

            Set<String> delegatedOrigins =
                    mCategory.showSites(SiteSettingsCategory.Type.NOTIFICATIONS)
                    ? TrustedWebActivityPermissionManager.get().getAllDelegatedOrigins()
                    : Collections.emptySet();

            for (WebsitePreference website : websites) {
                if (delegatedOrigins.contains(website.site().getAddress().getOrigin())) {
                    managedGroup.addPreference(website);
                    managed += 1;
                } else if (isOnBlockList(website)) {
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

            // The default, when the lists are shown for the first time, is for the
            // Blocked and Managed list to be collapsed and Allowed expanded -- because
            // the data in the Allowed list is normally more useful than the data in
            // the Blocked/Managed lists. A collapsed initial Blocked/Managed list works
            // well *except* when there's nothing in the Allowed list because then
            // there's only Blocked/Managed items to show and it doesn't make sense for
            // those items to be hidden. So, in those cases (and only when the lists are
            // shown for the first time) do we ignore the collapsed directive. The user
            // can still collapse and expand the Blocked/Managed list at will.
            if (mIsInitialRun) {
                if (mAllowedSiteCount == 0) {
                    if (blocked == 0 && managed > 0) {
                        mManagedListExpanded = true;
                    } else {
                        mBlockListExpanded = true;
                    }
                }
                mIsInitialRun = false;
            }

            if (!mBlockListExpanded) blockedGroup.removeAll();
            if (!mAllowListExpanded) allowedGroup.removeAll();
            if (!mManagedListExpanded) managedGroup.removeAll();
        }

        mWebsites = websites;
        updateBlockedHeader(blocked);
        updateAllowedHeader(mAllowedSiteCount, !isBlocked());
        updateManagedHeader(managed);

        return websites.size() != 0;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void filterSelectedDomains(Collection<Website> websites) {
        if (mSelectedDomains == null) {
            return;
        }
        for (Iterator<Website> it = websites.iterator(); it.hasNext();) {
            String domain =
                    UrlUtilities.getDomainAndRegistry(it.next().getAddress().getOrigin(), true);
            if (!mSelectedDomains.contains(domain)) {
                it.remove();
            }
        }
    }

    private boolean addChosenObjects(Collection<Website> sites) {
        Map<String, Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>>> objects =
                new HashMap<>();

        // Find chosen objects matching the current search and collect the list of sites
        // that have permission to access each.
        for (Website site : sites) {
            for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                if (mSearch == null || mSearch.isEmpty()
                        || info.getName().toLowerCase().contains(mSearch)) {
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
        updateManagedHeader(0);

        for (Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry : objects.values()) {
            Preference preference = new Preference(getStyledContext());
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
            return (triStateToggle.getCheckedSetting() == ContentSettingValues.BLOCK);
        } else {
            ChromeSwitchPreference binaryToggle =
                    (ChromeSwitchPreference) getPreferenceScreen().findPreference(
                            BINARY_TOGGLE_KEY);
            if (binaryToggle != null) return !binaryToggle.isChecked();
        }
        return false;
    }

    private void configureGlobalToggles() {
        int contentType = mCategory.getContentSettingsType();
        PreferenceScreen screen = getPreferenceScreen();

        // Find all preferences on the current preference screen. Some preferences are
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
        PreferenceGroup managedGroup = (PreferenceGroup) screen.findPreference(MANAGED_GROUP);
        boolean permissionBlockedByOs = mCategory.showPermissionBlockedMessage(getActivity());
        // For these categories, no binary, tri-state or custom toggles should be shown.
        boolean hideMainToggles = mCategory.showSites(SiteSettingsCategory.Type.ALL_SITES)
                || mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)
                || (permissionBlockedByOs
                        && !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH));
        boolean hideSecondaryToggles = hideMainToggles || permissionBlockedByOs;

        if (hideMainToggles) {
            screen.removePreference(binaryToggle);
            screen.removePreference(triStateToggle);
        } else if (mRequiresTriStateSetting) {
            screen.removePreference(binaryToggle);
            configureTriStateToggle(triStateToggle, contentType);
        } else {
            screen.removePreference(triStateToggle);
            configureBinaryToggle(binaryToggle, contentType);
        }

        if (permissionBlockedByOs) {
            maybeShowOsWarning(screen);
        }

        if (hideSecondaryToggles) {
            screen.removePreference(thirdPartyCookies);
            screen.removePreference(notificationsVibrate);
            screen.removePreference(explainProtectedMediaKey);
            screen.removePreference(allowedGroup);
            screen.removePreference(blockedGroup);
            screen.removePreference(managedGroup);
            // Since all preferences are hidden, there's nothing to do further and we can
            // simply return.
            return;
        }

        // Configure/hide the third-party cookies toggle, as needed.
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
            mListView.setFocusable(true);
        } else {
            // On small screens with no touch input, nested focusable items inside a LinearLayout in
            // ListView cause focus problems when using a keyboard (crbug.com/974413).
            // TODO(chouinard): Verify on a small screen device whether this patch is still needed
            // now that we've migrated this fragment to Support Library (mListView is a RecyclerView
            // now).
            mListView.setFocusable(false);
        }

        // When this menu opens, make sure the Blocked list is collapsed.
        if (!mGroupByAllowBlock) {
            mBlockListExpanded = false;
            mAllowListExpanded = true;
            mManagedListExpanded = false;
        }
        mGroupByAllowBlock = true;

        allowedGroup.setOnPreferenceClickListener(this);
        blockedGroup.setOnPreferenceClickListener(this);
        managedGroup.setOnPreferenceClickListener(this);
    }

    private void maybeShowOsWarning(PreferenceScreen screen) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH)
                && isBlocked()) {
            return;
        }

        // Show the link to system settings since permission is disabled.
        ChromeBasePreference osWarning = new ChromeBasePreference(getStyledContext(), null);
        ChromeBasePreference osWarningExtra = new ChromeBasePreference(getStyledContext(), null);
        mCategory.configurePermissionIsOffPreferences(
                osWarning, osWarningExtra, getActivity(), true);
        if (osWarning.getTitle() != null) {
            screen.addPreference(osWarning);
        }
        if (osWarningExtra.getTitle() != null) {
            screen.addPreference(osWarningExtra);
        }
    }

    private void configureTriStateToggle(
            TriStateSiteSettingsPreference triStateToggle, int contentType) {
        triStateToggle.setOnPreferenceChangeListener(this);
        @ContentSettingValues
        int setting = WebsitePreferenceBridge.getContentSetting(contentType);
        int[] descriptionIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(contentType);
        triStateToggle.initialize(setting, descriptionIds);
    }

    private void configureBinaryToggle(ChromeSwitchPreference binaryToggle, int contentType) {
        binaryToggle.setOnPreferenceChangeListener(this);
        binaryToggle.setTitle(ContentSettingsResources.getTitle(contentType));

        // Set summary on or off.
        if (mCategory.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)
                && WebsitePreferenceBridge.isLocationAllowedByPolicy()) {
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
            binaryToggle.setChecked(WebsitePreferenceBridge.isCategoryEnabled(contentType));
        }
    }

    private void updateThirdPartyCookiesCheckBox() {
        ChromeBaseCheckBoxPreference thirdPartyCookiesPref =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        THIRD_PARTY_COOKIES_TOGGLE_KEY);
        thirdPartyCookiesPref.setChecked(
                PrefServiceBridge.getInstance().getBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES));
        thirdPartyCookiesPref.setEnabled(
                WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.COOKIES));
        thirdPartyCookiesPref.setManagedPreferenceDelegate(preference
                -> PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.BLOCK_THIRD_PARTY_COOKIES));
    }

    private void updateNotificationsVibrateCheckBox() {
        ChromeBaseCheckBoxPreference preference =
                (ChromeBaseCheckBoxPreference) getPreferenceScreen().findPreference(
                        NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        if (preference != null) {
            preference.setEnabled(
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.NOTIFICATIONS));
        }
    }

    private void showManagedToast() {
        if (mCategory.isManagedByCustodian()) {
            ManagedPreferencesUtils.showManagedByParentToast(getActivity());
        } else {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity());
        }
    }
}
