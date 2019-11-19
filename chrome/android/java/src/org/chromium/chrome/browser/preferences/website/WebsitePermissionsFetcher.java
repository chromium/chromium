// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentSwitches;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;

/**
 * Utility class that asynchronously fetches any Websites and the permissions
 * that the user has set for them.
 */
public class WebsitePermissionsFetcher {
    private WebsitePreferenceBridge mWebsitePreferenceBridge;

    /**
     * A callback to pass to WebsitePermissionsFetcher. This is run when the
     * website permissions have been fetched.
     */
    public interface WebsitePermissionsCallback {
        void onWebsitePermissionsAvailable(Collection<Website> sites);
    }

    /**
     * A specialization of Pair to hold an (origin, embedder) tuple. This overrides
     * android.util.Pair#hashCode, which simply XORs the hashCodes of the pair of values together.
     * Having origin == embedder (a fix for a crash in crbug.com/636330) results in pathological
     * performance and causes Site Settings/All Sites to lag significantly on opening. See
     * crbug.com/732907.
     */
    public static class OriginAndEmbedder extends Pair<WebsiteAddress, WebsiteAddress> {
        public OriginAndEmbedder(WebsiteAddress origin, WebsiteAddress embedder) {
            super(origin, embedder);
        }

        public static OriginAndEmbedder create(WebsiteAddress origin, WebsiteAddress embedder) {
            return new OriginAndEmbedder(origin, embedder);
        }

        @Override
        public int hashCode() {
            // This is the calculation used by Arrays#hashCode().
            int result = 31 + (first == null ? 0 : first.hashCode());
            return 31 * result + (second == null ? 0 : second.hashCode());
        }
    }

    // This map looks up Websites by their origin and embedder.
    private final Map<OriginAndEmbedder, Website> mSites = new HashMap<>();

    private final boolean mFetchSiteImportantInfo;

    public WebsitePermissionsFetcher() {
        this(false);
    }

    /**
     * @param fetchSiteImportantInfo if the fetcher should query whether each site is 'important'.
     */
    public WebsitePermissionsFetcher(boolean fetchSiteImportantInfo) {
        mFetchSiteImportantInfo = fetchSiteImportantInfo;
        mWebsitePreferenceBridge = new WebsitePreferenceBridge();
    }

    /**
     * Fetches preferences for all sites that have them.
     * TODO(mvanouwerkerk): Add an argument |url| to only fetch permissions for
     * sites from the same origin as that of |url| - https://crbug.com/459222.
     * @param callback The callback to run when the fetch is complete.
     *
     * NB: you should call either this method or {@link #fetchPreferencesForCategory} only once per
     * instance.
     */
    public void fetchAllPreferences(WebsitePermissionsCallback callback) {
        TaskQueue queue = new TaskQueue();
        // Populate features from more specific to less specific.
        // Geolocation lookup permission is per-origin and per-embedder.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.GEOLOCATION));
        // Midi sysex access permission is per-origin and per-embedder.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.MIDI));
        // Cookies are stored per-host.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.COOKIES));
        // Local storage info is per-origin.
        queue.add(new LocalStorageInfoFetcher());
        // Website storage is per-host.
        queue.add(new WebStorageInfoFetcher());
        // Popup exceptions are host-based patterns (unless we start
        // synchronizing popup exceptions with desktop Chrome).
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.POPUPS));
        // Ads exceptions are host-based.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.ADS));
        // JavaScript exceptions are host-based patterns.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.JAVASCRIPT));
        // Sound exceptions are host-based patterns.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.SOUND));
        // Protected media identifier permission is per-origin and per-embedder.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER));
        // Notification permission is per-origin.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.NOTIFICATION));
        // Camera capture permission is per-origin and per-embedder.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.CAMERA));
        // Micropohone capture permission is per-origin and per-embedder.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.MICROPHONE));
        // Background sync permission is per-origin.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.BACKGROUND_SYNC));
        // Automatic Downloads permission is per-origin.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.AUTOMATIC_DOWNLOADS));
        // Autoplay permission is per-origin.
        queue.add(new ExceptionInfoFetcher(ContentSettingsType.AUTOPLAY));
        // USB device permission is per-origin and per-embedder.
        queue.add(new ChooserExceptionInfoFetcher(ContentSettingsType.USB_GUARD));
        // Clipboard info is per-origin.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.CLIPBOARD));
        // Sensors permission is per-origin.
        queue.add(new PermissionInfoFetcher(PermissionInfo.Type.SENSORS));
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES)) {
            // Bluetooth scanning permission is per-origin.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.BLUETOOTH_SCANNING));
        }
        if (ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC)) {
            // NFC permission is per-origin and per-embedder.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.NFC));
        }

        queue.add(new PermissionsAvailableCallbackRunner(callback));

        queue.next();
    }

    /**
     * Fetches all preferences within a specific category.
     *
     * @param category A category to fetch.
     * @param callback The callback to run when the fetch is complete.
     *
     * NB: you should call either this method or {@link #fetchAllPreferences} only once per
     * instance.
     */
    public void fetchPreferencesForCategory(SiteSettingsCategory category,
            WebsitePermissionsCallback callback) {
        if (category.showSites(SiteSettingsCategory.Type.ALL_SITES)) {
            fetchAllPreferences(callback);
            return;
        }

        TaskQueue queue = new TaskQueue();
        // Populate features from more specific to less specific.
        if (category.showSites(SiteSettingsCategory.Type.DEVICE_LOCATION)) {
            // Geolocation lookup permission is per-origin and per-embedder.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.GEOLOCATION));
        } else if (category.showSites(SiteSettingsCategory.Type.COOKIES)) {
            // Cookies exceptions are patterns.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.COOKIES));
        } else if (category.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            // Local storage info is per-origin.
            queue.add(new LocalStorageInfoFetcher());
            // Website storage is per-host.
            queue.add(new WebStorageInfoFetcher());
        } else if (category.showSites(SiteSettingsCategory.Type.CAMERA)) {
            // Camera capture permission is per-origin and per-embedder.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.CAMERA));
        } else if (category.showSites(SiteSettingsCategory.Type.MICROPHONE)) {
            // Micropohone capture permission is per-origin and per-embedder.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.MICROPHONE));
        } else if (category.showSites(SiteSettingsCategory.Type.POPUPS)) {
            // Popup exceptions are host-based patterns (unless we start
            // synchronizing popup exceptions with desktop Chrome.)
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.POPUPS));
        } else if (category.showSites(SiteSettingsCategory.Type.ADS)) {
            // Ads exceptions are host-based.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.ADS));
        } else if (category.showSites(SiteSettingsCategory.Type.JAVASCRIPT)) {
            // JavaScript exceptions are host-based patterns.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.JAVASCRIPT));
        } else if (category.showSites(SiteSettingsCategory.Type.SOUND)) {
            // Sound exceptions are host-based patterns.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.SOUND));
        } else if (category.showSites(SiteSettingsCategory.Type.NOTIFICATIONS)) {
            // Push notification permission is per-origin.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.NOTIFICATION));
        } else if (category.showSites(SiteSettingsCategory.Type.BACKGROUND_SYNC)) {
            // Background sync info is per-origin.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.BACKGROUND_SYNC));
        } else if (category.showSites(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS)) {
            // Automatic downloads info is per-origin.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.AUTOMATIC_DOWNLOADS));
        } else if (category.showSites(SiteSettingsCategory.Type.PROTECTED_MEDIA)) {
            // Protected media identifier permission is per-origin and per-embedder.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER));
        } else if (category.showSites(SiteSettingsCategory.Type.AUTOPLAY)) {
            // Autoplay permission is per-origin.
            queue.add(new ExceptionInfoFetcher(ContentSettingsType.AUTOPLAY));
        } else if (category.showSites(SiteSettingsCategory.Type.USB)) {
            // USB device permission is per-origin.
            queue.add(new ChooserExceptionInfoFetcher(ContentSettingsType.USB_GUARD));
        } else if (category.showSites(SiteSettingsCategory.Type.CLIPBOARD)) {
            // Clipboard permission is per-origin.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.CLIPBOARD));
        } else if (category.showSites(SiteSettingsCategory.Type.SENSORS)) {
            // Sensors permission is per-origin.
            queue.add(new PermissionInfoFetcher(PermissionInfo.Type.SENSORS));
        } else if (category.showSites(SiteSettingsCategory.Type.BLUETOOTH_SCANNING)) {
            CommandLine commandLine = CommandLine.getInstance();
            if (commandLine.hasSwitch(ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES)) {
                // Bluetooth scanning permission is per-origin.
                queue.add(new ExceptionInfoFetcher(ContentSettingsType.BLUETOOTH_SCANNING));
            }
        } else if (category.showSites(SiteSettingsCategory.Type.NFC)) {
            if (ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC)) {
                // NFC permission is per-origin and per-embedder.
                queue.add(new PermissionInfoFetcher(PermissionInfo.Type.NFC));
            }
        }
        queue.add(new PermissionsAvailableCallbackRunner(callback));
        queue.next();
    }

    private Website findOrCreateSite(String origin, String embedder) {
        // Avoid showing multiple entries in "All sites" for the same origin.
        if (embedder != null && (embedder.equals(origin) || "*".equals(embedder))) {
            embedder = null;
        }

        WebsiteAddress permissionOrigin = WebsiteAddress.create(origin);
        WebsiteAddress permissionEmbedder = WebsiteAddress.create(embedder);

        OriginAndEmbedder key = OriginAndEmbedder.create(permissionOrigin, permissionEmbedder);

        Website site = mSites.get(key);
        if (site == null) {
            site = new Website(permissionOrigin, permissionEmbedder);
            mSites.put(key, site);
        }
        return site;
    }

    private void setException(int contentSettingsType) {
        @ContentSettingException.Type
        int exceptionType;
        for (exceptionType = 0; exceptionType < ContentSettingException.Type.NUM_ENTRIES;
                exceptionType++) {
            if (contentSettingsType
                    == ContentSettingException.getContentSettingsType(exceptionType)) {
                break;
            }
        }
        assert contentSettingsType
                == ContentSettingException.getContentSettingsType(exceptionType)
            : "Unexpected content setting type received: "
                        + contentSettingsType;

        for (ContentSettingException exception :
                mWebsitePreferenceBridge.getContentSettingsExceptions(contentSettingsType)) {
            // The pattern "*" represents the default setting, not a specific website.
            if (exception.getPattern().equals("*")) continue;
            String address = exception.getPattern();
            if (address == null) continue;
            Website site = findOrCreateSite(address, null);
            site.setContentSettingException(exceptionType, exception);
        }
    }

    /**
     * A single task in the WebsitePermissionsFetcher task queue. We need fetching of features to be
     * serialized, as we need to have all the origins in place prior to populating the hosts.
     */
    private abstract class Task {
        /** Override this method to implement a synchronous task. */
        void run() {}

        /**
         * Override this method to implement an asynchronous task. Call queue.next() once execution
         * is complete.
         */
        void runAsync(TaskQueue queue) {
            run();
            queue.next();
        }
    }

    /**
     * A queue used to store the sequence of tasks to run to fetch the website preferences. Each
     * task is run sequentially, and some of the tasks may run asynchronously.
     */
    private static class TaskQueue extends LinkedList<Task> {
        void next() {
            if (!isEmpty()) removeFirst().runAsync(this);
        }
    }

    private class PermissionInfoFetcher extends Task {
        final @PermissionInfo.Type int mType;

        public PermissionInfoFetcher(@PermissionInfo.Type int type) {
            mType = type;
        }

        @Override
        public void run() {
            for (PermissionInfo info : mWebsitePreferenceBridge.getPermissionInfo(mType)) {
                String origin = info.getOrigin();
                if (origin == null) continue;
                String embedder = mType == PermissionInfo.Type.SENSORS ? null : info.getEmbedder();
                findOrCreateSite(origin, embedder).setPermissionInfo(info);
            }
        }
    }

    private class ChooserExceptionInfoFetcher extends Task {
        final @ContentSettingsType int mChooserDataType;

        public ChooserExceptionInfoFetcher(@ContentSettingsType int type) {
            mChooserDataType = SiteSettingsCategory.objectChooserDataTypeFromGuard(type);
        }

        @Override
        public void run() {
            if (mChooserDataType == -1) return;

            for (ChosenObjectInfo info :
                    mWebsitePreferenceBridge.getChosenObjectInfo(mChooserDataType)) {
                String origin = info.getOrigin();
                if (origin == null) continue;
                findOrCreateSite(origin, info.getEmbedder()).addChosenObjectInfo(info);
            }
        }
    }

    private class ExceptionInfoFetcher extends Task {
        final int mContentSettingsType;

        public ExceptionInfoFetcher(int contentSettingsType) {
            mContentSettingsType = contentSettingsType;
        }

        @Override
        public void run() {
            setException(mContentSettingsType);
        }
    }

    private class LocalStorageInfoFetcher extends Task {
        @Override
        public void runAsync(final TaskQueue queue) {
            mWebsitePreferenceBridge.fetchLocalStorageInfo(new Callback<HashMap>() {
                @Override
                public void onResult(HashMap result) {
                    for (Object o : result.entrySet()) {
                        @SuppressWarnings("unchecked")
                        Map.Entry<String, LocalStorageInfo> entry =
                                (Map.Entry<String, LocalStorageInfo>) o;
                        String address = entry.getKey();
                        if (address == null) continue;
                        findOrCreateSite(address, null).setLocalStorageInfo(entry.getValue());
                    }
                    queue.next();
                }
            }, mFetchSiteImportantInfo);
        }
    }

    private class WebStorageInfoFetcher extends Task {
        @Override
        public void runAsync(final TaskQueue queue) {
            mWebsitePreferenceBridge.fetchStorageInfo(new Callback<ArrayList>() {
                @Override
                public void onResult(ArrayList result) {
                    @SuppressWarnings("unchecked")
                    ArrayList<StorageInfo> infoArray = result;

                    for (StorageInfo info : infoArray) {
                        String address = info.getHost();
                        if (address == null) continue;
                        findOrCreateSite(address, null).addStorageInfo(info);
                    }
                    queue.next();
                }
            });
        }
    }

    private class PermissionsAvailableCallbackRunner extends Task {
        private final WebsitePermissionsCallback mCallback;

        private PermissionsAvailableCallbackRunner(WebsitePermissionsCallback callback) {
            mCallback = callback;
        }

        @Override
        public void run() {
            mCallback.onWebsitePermissionsAvailable(mSites.values());
        }
    }

    @VisibleForTesting
    public void setWebsitePreferenceBridgeForTesting(
            WebsitePreferenceBridge websitePreferenceBridge) {
        mWebsitePreferenceBridge = websitePreferenceBridge;
    }
}
