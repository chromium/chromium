// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.provider.Settings;
import android.support.v7.app.AlertDialog;
import android.text.format.Formatter;
import android.widget.ListAdapter;
import android.widget.ListView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.components.url_formatter.UrlFormatter;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * Shows the permissions and other settings for a particular website.
 */
public class SingleWebsitePreferences extends PreferenceFragment
        implements OnPreferenceChangeListener, OnPreferenceClickListener {
    // SingleWebsitePreferences expects either EXTRA_SITE (a Website) or
    // EXTRA_SITE_ADDRESS (a WebsiteAddress) to be present (but not both). If
    // EXTRA_SITE is present, the fragment will display the permissions in that
    // Website object. If EXTRA_SITE_ADDRESS is present, the fragment will find all
    // permissions for that website address and display those.
    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";
    public static final String EXTRA_SITE_ADDRESS = "org.chromium.chrome.preferences.site_address";
    public static final String EXTRA_OBJECT_INFO = "org.chromium.chrome.preferences.object_info";

    // Preference keys, see single_website_preferences.xml
    // Headings:
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_USAGE = "site_usage";
    public static final String PREF_PERMISSIONS = "site_permissions";
    public static final String PREF_OS_PERMISSIONS_WARNING = "os_permissions_warning";
    public static final String PREF_OS_PERMISSIONS_WARNING_EXTRA = "os_permissions_warning_extra";
    public static final String PREF_OS_PERMISSIONS_WARNING_DIVIDER =
            "os_permissions_warning_divider";
    public static final String PREF_INTRUSIVE_ADS_INFO = "intrusive_ads_info";
    public static final String PREF_INTRUSIVE_ADS_INFO_DIVIDER = "intrusive_ads_info_divider";
    // Actions at the top (if adding new, see hasUsagePreferences below):
    public static final String PREF_CLEAR_DATA = "clear_data";
    // Buttons:
    public static final String PREF_RESET_SITE = "reset_site_button";

    // Website permissions (if adding new, see hasPermissionsPreferences and resetSite below)
    // All permissions from the permissions preference category must be listed here.
    private static final String[] PERMISSION_PREFERENCE_KEYS = {
            // Permission keys mapped for next {@link ContentSettingException.Type} values.
            "ads_permission_list", // ContentSettingException.Type.ADS
            "autoplay_permission_list", // ContentSettingException.Type.AUTOPLAY
            "background_sync_permission_list", // ContentSettingException.Type.BACKGROUND_SYNC
            "automatic_downloads_permission_list",
            // ContentSettingException.Type.AUTOMATIC_DOWNLOADS
            "cookies_permission_list", // ContentSettingException.Type.COOKIE
            "javascript_permission_list", // ContentSettingException.Type.JAVASCRIPT
            "popup_permission_list", // ContentSettingException.Type.POPUP
            "sound_permission_list", // ContentSettingException.Type.SOUND
            // Permission keys mapped for next {@link PermissionInfo.Type} values.
            "camera_permission_list", // PermissionInfo.Type.CAMERA
            "clipboard_permission_list", // PermissionInfo.Type.CLIPBOARD
            "location_access_list", // PermissionInfo.Type.GEOLOCATION
            "microphone_permission_list", // PermissionInfo.Type.MICROPHONE
            "midi_sysex_permission_list", // PermissionInfo.Type.MIDI
            "push_notifications_list", // PermissionInfo.Type.NOTIFICATION
            "protected_media_identifier_permission_list",
            // PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER
            "sensors_permission_list", // PermissionInfo.Type.SENSORS
    };

    private static final int REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS = 1;

    private final SiteDataCleaner mSiteDataCleaner = new SiteDataCleaner();

    // The website this page is displaying details about.
    private Website mSite;

    // The number of chosen object permissions displayed.
    private int mObjectPermissionCount;

    private class SingleWebsitePermissionsPopulator
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        private final WebsiteAddress mSiteAddress;

        public SingleWebsitePermissionsPopulator(WebsiteAddress siteAddress) {
            mSiteAddress = siteAddress;
        }

        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // TODO(mvanouwerkerk): Avoid modifying the outer class from this inner class.
            mSite = mergePermissionInfoForTopLevelOrigin(mSiteAddress, sites);

            displaySitePermissions();
        }
    }

    private final Runnable mDataClearedCallback = () -> {
        Activity activity = getActivity();
        if (activity == null || activity.isFinishing()) {
            return;
        }
        removePreferenceSafely(PREF_CLEAR_DATA);
        if (!hasUsagePreferences()) {
            removePreferenceSafely(PREF_USAGE);
        }
        popBackIfNoSettings();
    };

    /**
     * Creates a Bundle with the correct arguments for opening this fragment for
     * the website with the given url.
     *
     * @param url The URL to open the fragment with. This is a complete url including scheme,
     *            domain, port,  path, etc.
     * @return The bundle to attach to the preferences intent.
     */
    public static Bundle createFragmentArgsForSite(String url) {
        Bundle fragmentArgs = new Bundle();
        // TODO(mvanouwerkerk): Define a pure getOrigin method in UrlUtilities that is the
        // equivalent of the call below, because this is perfectly fine for non-display purposes.
        String origin = UrlFormatter.formatUrlForSecurityDisplay(url);
        fragmentArgs.putSerializable(EXTRA_SITE_ADDRESS, WebsiteAddress.create(origin));
        return fragmentArgs;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        getActivity().setTitle(R.string.prefs_site_settings);
        ListView listView = (ListView) getView().findViewById(android.R.id.list);
        listView.setDivider(null);

        Object extraSite = getArguments().getSerializable(EXTRA_SITE);
        Object extraSiteAddress = getArguments().getSerializable(EXTRA_SITE_ADDRESS);

        if (extraSite != null && extraSiteAddress == null) {
            mSite = (Website) extraSite;
            displaySitePermissions();
        } else if (extraSiteAddress != null && extraSite == null) {
            WebsitePermissionsFetcher fetcher;
            fetcher = new WebsitePermissionsFetcher();
            fetcher.fetchAllPreferences(
                    new SingleWebsitePermissionsPopulator((WebsiteAddress) extraSiteAddress));
        } else {
            assert false : "Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.";
        }

        super.onActivityCreated(savedInstanceState);
    }

    /**
     * Given an address and a list of sets of websites, returns a new site with the same origin
     * as |address| which has merged into it the permissions of the matching input sites. If a
     * permission is found more than once, the one found first is used and the latter are ignored.
     * This should not drop any relevant data as there should not be duplicates like that in the
     * first place.
     *
     * @param address The address to search for.
     * @param websites The websites to search in.
     * @return The merged website.
     */
    private static Website mergePermissionInfoForTopLevelOrigin(
            WebsiteAddress address, Collection<Website> websites) {
        String origin = address.getOrigin();
        String host = Uri.parse(origin).getHost();
        Website merged = new Website(address, null);
        // This loop looks expensive, but the amount of data is likely to be relatively small
        // because most sites have very few permissions.
        for (Website other : websites) {
            if (merged.getContentSettingException(ContentSettingException.Type.ADS) == null
                    && other.getContentSettingException(ContentSettingException.Type.ADS) != null
                    && other.compareByAddressTo(merged) == 0) {
                merged.setContentSettingException(ContentSettingException.Type.ADS,
                        other.getContentSettingException(ContentSettingException.Type.ADS));
            }
            for (@PermissionInfo.Type int type = 0; type < PermissionInfo.Type.NUM_ENTRIES;
                    type++) {
                if (merged.getPermissionInfo(type) == null && other.getPermissionInfo(type) != null
                        && permissionInfoIsForTopLevelOrigin(
                                   other.getPermissionInfo(type), origin)) {
                    merged.setPermissionInfo(other.getPermissionInfo(type));
                }
            }
            if (merged.getLocalStorageInfo() == null
                    && other.getLocalStorageInfo() != null
                    && origin.equals(other.getLocalStorageInfo().getOrigin())) {
                merged.setLocalStorageInfo(other.getLocalStorageInfo());
            }
            for (StorageInfo storageInfo : other.getStorageInfo()) {
                if (host.equals(storageInfo.getHost())) {
                    merged.addStorageInfo(storageInfo);
                }
            }
            for (ChosenObjectInfo objectInfo : other.getChosenObjectInfo()) {
                if (origin.equals(objectInfo.getOrigin())
                        && (objectInfo.getEmbedder() == null
                                   || objectInfo.getEmbedder().equals("*"))) {
                    merged.addChosenObjectInfo(objectInfo);
                }
            }
            if (host.equals(other.getAddress().getHost())) {
                for (@ContentSettingException.Type int type = 0;
                        type < ContentSettingException.Type.NUM_ENTRIES; type++) {
                    if (type == ContentSettingException.Type.ADS
                            || type == ContentSettingException.Type.COOKIE) {
                        continue;
                    }
                    if (merged.getContentSettingException(type) == null
                            && other.getContentSettingException(type) != null) {
                        merged.setContentSettingException(
                                type, other.getContentSettingException(type));
                    }
                }
            }

            // TODO(crbug.com/763982): Deal with this TODO colony.
            // TODO(mvanouwerkerk): Make the various info types share a common interface that
            // supports reading the origin or host.
            // TODO(lshang): Merge in CookieException? It will use patterns.
        }
        return merged;
    }

    private static boolean permissionInfoIsForTopLevelOrigin(PermissionInfo info, String origin) {
        // TODO(mvanouwerkerk): Find a more generic place for this method.
        return origin.equals(info.getOrigin())
                && (origin.equals(info.getEmbedderSafe()) || "*".equals(info.getEmbedderSafe()));
    }

    /**
     * Updates the permissions displayed in the UI by fetching them from mSite.
     * Must only be called once mSite is set.
     */
    private void displaySitePermissions() {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.single_website_preferences);

        Set<String> permissionPreferenceKeys =
                new HashSet<>(Arrays.asList(PERMISSION_PREFERENCE_KEYS));
        int maxPermissionOrder = 0;
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        ListAdapter preferences = preferenceScreen.getRootAdapter();
        for (int i = 0; i < preferences.getCount(); ++i) {
            Preference preference = (Preference) preferences.getItem(i);
            setUpPreference(preference);
            // Keep track of the maximum 'order' value of permission preferences, to allow correct
            // positioning of subsequent permission preferences.
            if (permissionPreferenceKeys.contains(preference.getKey())) {
                maxPermissionOrder = Math.max(maxPermissionOrder, preference.getOrder());
            }
        }
        setUpChosenObjectPreferences(maxPermissionOrder);
        setUpOsWarningPreferences();

        setUpAdsInformationalBanner();

        // Remove categories if no sub-items.
        if (!hasUsagePreferences()) {
            removePreferenceSafely(PREF_USAGE);
        }
        if (!hasPermissionsPreferences()) {
            removePreferenceSafely(PREF_PERMISSIONS);
        }
    }

    private void setUpPreference(Preference preference) {
        if (PREF_SITE_TITLE.equals(preference.getKey())) {
            preference.setTitle(mSite.getTitle());
        } else if (PREF_CLEAR_DATA.equals(preference.getKey())) {
            setUpClearDataPreference(preference);
        } else if (PREF_RESET_SITE.equals(preference.getKey())) {
            preference.setOnPreferenceClickListener(this);
        } else {
            assert PERMISSION_PREFERENCE_KEYS.length
                    == ContentSettingException.Type.NUM_ENTRIES + PermissionInfo.Type.NUM_ENTRIES;
            for (@ContentSettingException.Type int i = 0;
                    i < ContentSettingException.Type.NUM_ENTRIES; i++) {
                if (!PERMISSION_PREFERENCE_KEYS[i].equals(preference.getKey())) {
                    continue;
                }
                if (i == ContentSettingException.Type.ADS) {
                    setUpAdsPreference(preference);
                } else if (i == ContentSettingException.Type.SOUND) {
                    setUpSoundPreference(preference);
                } else {
                    setUpListPreference(preference, mSite.getContentSettingPermission(i));
                }
                return;
            }
            for (@PermissionInfo.Type int i = 0; i < PermissionInfo.Type.NUM_ENTRIES; i++) {
                if (!PERMISSION_PREFERENCE_KEYS[i + ContentSettingException.Type.NUM_ENTRIES]
                                .equals(preference.getKey())) {
                    continue;
                }
                if (i == PermissionInfo.Type.GEOLOCATION) {
                    setUpLocationPreference(preference);
                } else if (i == PermissionInfo.Type.NOTIFICATION) {
                    setUpNotificationsPreference(preference);
                } else {
                    setUpListPreference(preference, mSite.getPermission(i));
                }
                return;
            }
        }
    }

    private void setUpClearDataPreference(Preference preference) {
        long usage = mSite.getTotalUsage();
        if (usage > 0) {
            Context context = preference.getContext();
            preference.setTitle(
                    String.format(context.getString(R.string.origin_settings_storage_usage_brief),
                            Formatter.formatShortFileSize(context, usage)));
            ((ClearWebsiteStorage) preference)
                    .setConfirmationListener(new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            mSite.clearAllStoredData(mDataClearedCallback::run);
                        }
                    });
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }

    private void setUpNotificationsPreference(Preference preference) {
        final ContentSetting value = mSite.getPermission(PermissionInfo.Type.NOTIFICATION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (!(value == ContentSetting.ALLOW || value == ContentSetting.BLOCK)) {
                // TODO(crbug.com/735110): Figure out if this is the correct thing to do, for values
                // that are non-null, but not ALLOW or BLOCK either. (In setupListPreference we
                // treat non-ALLOW settings as BLOCK, but here we are simply removing them.)
                getPreferenceScreen().removePreference(preference);
                return;
            }
            // On Android O this preference is read-only, so we replace the existing pref with a
            // regular Preference that takes users to OS settings on click.
            Preference newPreference = new Preference(preference.getContext());
            newPreference.setKey(preference.getKey());
            setUpPreferenceCommon(newPreference);

            if (isPermissionControlledByDSE(
                        ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS)) {
                newPreference.setSummary(getResources().getString(value == ContentSetting.ALLOW
                                ? R.string.website_settings_permissions_allow_dse
                                : R.string.website_settings_permissions_block_dse));
            } else {
                newPreference.setSummary(
                        getResources().getString(ContentSettingsResources.getSiteSummary(value)));
            }

            newPreference.setDefaultValue(value);

            // This preference is read-only so should not attempt to persist to shared prefs.
            newPreference.setPersistent(false);

            newPreference.setOnPreferenceClickListener(new OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(Preference preference) {
                    // There is no guarantee that a channel has been initialized yet for sites
                    // that were granted permission before the channel-initialization-on-grant
                    // code was in place. However, getChannelIdForOrigin will fall back to the
                    // generic Sites channel if no specific channel has been created for the given
                    // origin, so it is safe to open the channel settings for whatever channel ID
                    // it returns.
                    String channelId = SiteChannelsManager.getInstance().getChannelIdForOrigin(
                            mSite.getAddress().getOrigin());
                    launchOsChannelSettings(preference.getContext(), channelId);
                    return true;
                }
            });
            newPreference.setOrder(preference.getOrder());
            getPreferenceScreen().removePreference(preference);
            getPreferenceScreen().addPreference(newPreference);
        } else {
            setUpListPreference(preference, value);
            if (isPermissionControlledByDSE(ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
                    && value != null) {
                updatePreferenceForDSESetting(preference);
            }
        }
    }

    private void launchOsChannelSettings(Context context, String channelId) {
        Intent intent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
        intent.putExtra(Settings.EXTRA_CHANNEL_ID, channelId);
        intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        startActivityForResult(intent, REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS);
    }

    /**
     * If we are returning to Site Settings from another activity, the preferences displayed may be
     * out of date. Here we refresh any we suspect may have changed.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        // The preference screen and mSite may be null if this activity was killed in the
        // background, and the tasks scheduled from onActivityCreated haven't completed yet. Those
        // tasks will take care of reinitializing everything afresh so there is no work to do here.
        if (getPreferenceScreen() == null || mSite == null) {
            return;
        }
        if (requestCode == REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS) {
            // User has navigated back from system channel settings on O+. Ensure notification
            // preference is up to date, since they might have toggled it from channel settings.
            Preference notificationsPreference = getPreferenceScreen().findPreference(
                    PERMISSION_PREFERENCE_KEYS[PermissionInfo.Type.NOTIFICATION
                            + ContentSettingException.Type.NUM_ENTRIES]);
            if (notificationsPreference != null) {
                setUpNotificationsPreference(notificationsPreference);
            }
        }
    }

    private void setUpChosenObjectPreferences(int maxPermissionOrder) {
        for (ChosenObjectInfo info : mSite.getChosenObjectInfo()) {
            Preference preference = new Preference(getActivity());
            preference.getExtras().putSerializable(EXTRA_OBJECT_INFO, info);
            preference.setIcon(ContentSettingsResources.getIcon(info.getContentSettingsType()));
            preference.setOnPreferenceClickListener(this);
            preference.setOrder(maxPermissionOrder);
            preference.setTitle(info.getName());
            preference.setWidgetLayoutResource(R.layout.object_permission);
            getPreferenceScreen().addPreference(preference);
            mObjectPermissionCount++;
        }
    }

    private void setUpOsWarningPreferences() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        SiteSettingsCategory categoryWithWarning = getWarningCategory();
        // Remove the 'permission is off in Android' message if not needed.
        if (categoryWithWarning == null) {
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_DIVIDER);
        } else {
            Preference osWarning = preferenceScreen.findPreference(PREF_OS_PERMISSIONS_WARNING);
            Preference osWarningExtra =
                    preferenceScreen.findPreference(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            categoryWithWarning.configurePermissionIsOffPreferences(
                    osWarning, osWarningExtra, getActivity(), false);
            if (osWarning.getTitle() == null) {
                preferenceScreen.removePreference(osWarning);
            } else if (osWarningExtra.getTitle() == null) {
                preferenceScreen.removePreference(osWarningExtra);
            }
        }
    }

    private void setUpAdsInformationalBanner() {
        // Add the informational banner which shows at the top of the UI if ad blocking is
        // activated on this site.
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        boolean adBlockingActivated = SiteSettingsCategory.adsCategoryEnabled()
                && WebsitePreferenceBridge.getAdBlockingActivated(mSite.getAddress().getOrigin())
                && preferenceScreen.findPreference(
                           PERMISSION_PREFERENCE_KEYS[ContentSettingException.Type.ADS])
                        != null;

        if (!adBlockingActivated) {
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO);
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO_DIVIDER);
        }
    }

    private SiteSettingsCategory getWarningCategory() {
        // If more than one per-app permission is disabled in Android, we can pick any category to
        // show the warning, because they will all show the same warning and all take the user to
        // the user to the same location. It is preferrable, however, that we give Geolocation some
        // priority because that category is the only one that potentially shows an additional
        // warning (when Location is turned off globally).
        if (showWarningFor(SiteSettingsCategory.Type.DEVICE_LOCATION)) {
            return SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.DEVICE_LOCATION);
        } else if (showWarningFor(SiteSettingsCategory.Type.CAMERA)) {
            return SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.CAMERA);
        } else if (showWarningFor(SiteSettingsCategory.Type.MICROPHONE)) {
            return SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.MICROPHONE);
        } else if (showWarningFor(SiteSettingsCategory.Type.NOTIFICATIONS)) {
            return SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.NOTIFICATIONS);
        }
        return null;
    }

    private boolean showWarningFor(@SiteSettingsCategory.Type int type) {
        for (int i = 0; i < PermissionInfo.Type.NUM_ENTRIES; i++) {
            if (PermissionInfo.getContentSettingsType(i)
                    == SiteSettingsCategory.contentSettingsType(type)) {
                return mSite.getPermission(i) == null
                        ? false
                        : SiteSettingsCategory.createFromType(type).showPermissionBlockedMessage(
                                  getActivity());
            }
        }
        return false;
    }

    private boolean hasUsagePreferences() {
        // New actions under the Usage preference category must be listed here so that the category
        // heading can be removed when no actions are shown.
        return getPreferenceScreen().findPreference(PREF_CLEAR_DATA) != null;
    }

    private boolean hasPermissionsPreferences() {
        if (mObjectPermissionCount > 0) return true;
        PreferenceScreen screen = getPreferenceScreen();
        for (String key : PERMISSION_PREFERENCE_KEYS) {
            if (screen.findPreference(key) != null) return true;
        }
        return false;
    }

    /**
     * Initialize a ListPreference with a certain value.
     * @param preference The ListPreference to initialize.
     * @param value The value to initialize it to.
     */
    private void setUpListPreference(Preference preference, ContentSetting value) {
        if (value == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }
        setUpPreferenceCommon(preference);
        ListPreference listPreference = (ListPreference) preference;

        CharSequence[] keys = new String[2];
        CharSequence[] descriptions = new String[2];
        keys[0] = ContentSetting.ALLOW.toString();
        keys[1] = ContentSetting.BLOCK.toString();
        descriptions[0] = getResources().getString(
                ContentSettingsResources.getSiteSummary(ContentSetting.ALLOW));
        descriptions[1] = getResources().getString(
                ContentSettingsResources.getSiteSummary(ContentSetting.BLOCK));
        listPreference.setEntryValues(keys);
        listPreference.setEntries(descriptions);
        // TODO(crbug.com/735110): Figure out if this is the correct thing to do - here we are
        // effectively treating non-ALLOW values as BLOCK.
        int index = (value == ContentSetting.ALLOW ? 0 : 1);
        listPreference.setValueIndex(index);
        listPreference.setOnPreferenceChangeListener(this);
        listPreference.setSummary("%s");
    }

    /**
     * Sets some properties that apply to both regular Preferences and ListPreferences, i.e.
     * preference title, enabled-state, and icon, based on the preference's key.
     */
    private void setUpPreferenceCommon(Preference preference) {
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        int explanationResourceId = ContentSettingsResources.getExplanation(contentType);
        if (explanationResourceId != 0) {
            preference.setTitle(explanationResourceId);
        }
        if (!preference.isEnabled()) {
            preference.setIcon(
                    ContentSettingsResources.getDisabledIcon(contentType, getResources()));
            return;
        }
        SiteSettingsCategory category =
                SiteSettingsCategory.createFromContentSettingsType(contentType);
        if (category != null && !category.enabledInAndroid(getActivity())) {
            preference.setIcon(category.getDisabledInAndroidIcon(getActivity()));
            preference.setEnabled(false);
        } else {
            preference.setIcon(PreferenceUtils.getTintedIcon(
                    getActivity(), ContentSettingsResources.getIcon(contentType)));
        }
    }

    private void setUpLocationPreference(Preference preference) {
        ContentSetting permission = mSite.getPermission(PermissionInfo.Type.GEOLOCATION);
        setUpListPreference(preference, permission);
        if (isPermissionControlledByDSE(ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION)
                && permission != null) {
            updatePreferenceForDSESetting(preference);
        }
    }

    private void setUpSoundPreference(Preference preference) {
        ContentSetting currentValue =
                mSite.getContentSettingPermission(ContentSettingException.Type.SOUND);
        // In order to always show the sound permission, set it up with the default value if it
        // doesn't have a current value.
        if (currentValue == null) {
            currentValue = PrefServiceBridge.getInstance().isCategoryEnabled(
                                   ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND)
                    ? ContentSetting.ALLOW
                    : ContentSetting.BLOCK;
        }
        setUpListPreference(preference, currentValue);
    }

    /**
     * Updates the ads list preference based on whether the site is a candidate for blocking. This
     * has some custom behavior.
     * 1. If the site is a candidate and has activation, the permission should show up even if it
     *    is set as the default (e.g. |preference| is null).
     * 2. The BLOCK string is custom.
     */
    private void setUpAdsPreference(Preference preference) {
        // Do not show the setting if the category is not enabled.
        if (!SiteSettingsCategory.adsCategoryEnabled()) {
            setUpListPreference(preference, null);
            return;
        }
        // If the ad blocker is activated, then this site will have ads blocked unless there is an
        // explicit permission disallowing the blocking.
        boolean activated =
                WebsitePreferenceBridge.getAdBlockingActivated(mSite.getAddress().getOrigin());
        ContentSetting permission =
                mSite.getContentSettingPermission(ContentSettingException.Type.ADS);

        // If |permission| is null, there is no explicit (non-default) permission set for this site.
        // If the site is not considered a candidate for blocking, do the standard thing and remove
        // the preference.
        if (permission == null && !activated) {
            setUpListPreference(preference, null);
            return;
        }

        // However, if the blocking is activated, we still want to show the permission, even if it
        // is in the default state.
        if (permission == null) {
            permission = PrefServiceBridge.getInstance().isCategoryEnabled(
                                 ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS)
                    ? ContentSetting.ALLOW
                    : ContentSetting.BLOCK;
        }
        setUpListPreference(preference, permission);

        // The subresource filter permission has a custom BLOCK string.
        ListPreference listPreference = (ListPreference) preference;
        Resources res = getResources();
        listPreference.setEntries(
                new String[] {res.getString(R.string.website_settings_permissions_allow),
                        res.getString(R.string.website_settings_permissions_ads_block)});
        listPreference.setValueIndex(permission == ContentSetting.ALLOW ? 0 : 1);
    }

    /**
     * Returns true if the DSE (default search engine) geolocation and notifications permissions
     * are configured for the DSE.
     */
    private boolean isPermissionControlledByDSE(@ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridge.isPermissionControlledByDSE(
                contentSettingsType, mSite.getAddress().getOrigin(), false);
    }

    /**
     * Updates the location preference to indicate that the site has access to location (via X-Geo)
     * for searches that happen from the omnibox.
     * @param preference The Location preference to modify.
     */
    private void updatePreferenceForDSESetting(Preference preference) {
        ListPreference listPreference = (ListPreference) preference;
        Resources res = getResources();
        listPreference.setEntries(new String[] {
                res.getString(R.string.website_settings_permissions_allow_dse),
                res.getString(R.string.website_settings_permissions_block_dse),
        });
    }

    private int getContentSettingsTypeFromPreferenceKey(String preferenceKey) {
        for (int i = 0; i < PERMISSION_PREFERENCE_KEYS.length; i++) {
            if (PERMISSION_PREFERENCE_KEYS[i].equals(preferenceKey)) {
                return i < ContentSettingException.Type.NUM_ENTRIES
                        ? ContentSettingException.getContentSettingsType(i)
                        : PermissionInfo.getContentSettingsType(
                                  i - ContentSettingException.Type.NUM_ENTRIES);
            }
        }
        return 0;
    }

    private void popBackIfNoSettings() {
        if (!hasPermissionsPreferences() && !hasUsagePreferences() && getActivity() != null) {
            getActivity().finish();
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        ContentSetting permission = ContentSetting.fromString((String) newValue);
        for (int i = 0; i < PERMISSION_PREFERENCE_KEYS.length; i++) {
            if (PERMISSION_PREFERENCE_KEYS[i].equals(preference.getKey())) {
                if (i < ContentSettingException.Type.NUM_ENTRIES) {
                    mSite.setContentSettingPermission(i, permission);
                } else {
                    mSite.setPermission(i - ContentSettingException.Type.NUM_ENTRIES, permission);
                }
                return true;
            }
        }
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        Bundle extras = preference.peekExtras();
        if (extras != null) {
            ChosenObjectInfo objectInfo =
                    (ChosenObjectInfo) extras.getSerializable(EXTRA_OBJECT_INFO);
            if (objectInfo != null) {
                objectInfo.revoke();

                PreferenceScreen preferenceScreen = getPreferenceScreen();
                preferenceScreen.removePreference(preference);
                mObjectPermissionCount--;
                if (!hasPermissionsPreferences()) {
                    Preference heading = preferenceScreen.findPreference(PREF_PERMISSIONS);
                    preferenceScreen.removePreference(heading);
                }
                return true;
            }
        }

        // Handle the Clear & Reset preference click by showing a confirmation.
        new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                .setTitle(R.string.website_reset)
                .setMessage(R.string.website_reset_confirmation)
                .setPositiveButton(R.string.website_reset, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        resetSite();
                    }
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
        return true;
    }

    /**
     * Resets the current site, clearing all permissions and storage used (inc. cookies).
     */
    @VisibleForTesting
    protected void resetSite() {
        if (getActivity() == null) return;
        // Clear the screen.
        // TODO(mvanouwerkerk): Refactor this class so that it does not depend on the screen state
        // for its logic. This class should maintain its own data model, and only update the screen
        // after a change is made.
        for (String key : PERMISSION_PREFERENCE_KEYS) {
            removePreferenceSafely(key);
        }

        mObjectPermissionCount = 0;

        // Clearing stored data implies popping back to parent menu if there
        // is nothing left to show. Therefore, we only need to explicitly
        // close the activity if there's no stored data to begin with.
        boolean finishActivityImmediately = mSite.getTotalUsage() == 0;

        mSiteDataCleaner.clearData(mSite, mDataClearedCallback);

        if (finishActivityImmediately) {
            getActivity().finish();
        }
    }

    /**
     * Ensures preference exists before removing to avoid NPE in
     * {@link PreferenceScreen#removePreference}.
     */
    private void removePreferenceSafely(CharSequence prefKey) {
        PreferenceScreen screen = getPreferenceScreen();
        Preference preference = screen.findPreference(prefKey);
        if (preference != null) screen.removePreference(preference);
    }
}
