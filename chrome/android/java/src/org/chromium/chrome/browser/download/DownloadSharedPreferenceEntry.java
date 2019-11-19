// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;

import java.util.UUID;

/**
 * Class representing the download information stored in SharedPreferences to construct a
 * download notification.
 */
public class DownloadSharedPreferenceEntry {
    private static final String TAG = "DownloadEntry";

    // Current version of the DownloadSharedPreferenceEntry. When changing the SharedPreference,
    // we need to change the version number too.
    @VisibleForTesting
    static final int VERSION = 6;

    public final int notificationId;
    public final boolean isOffTheRecord;  // Whether the download is public (non incognito).
    public final boolean canDownloadWhileMetered;
    public final String fileName;
    // This can only be false for paused downloads. For downloads that are pending or in progress,
    // isAutoResumable should always be true.
    public final boolean isAutoResumable;
    public final ContentId id;
    public final boolean isTransient;

    static final DownloadSharedPreferenceEntry INVALID_ENTRY =
            new DownloadSharedPreferenceEntry(new ContentId(), -1, false, false, "", false, false);

    DownloadSharedPreferenceEntry(ContentId id, int notificationId, boolean isOffTheRecord,
            boolean canDownloadWhileMetered, String fileName, boolean isAutoResumable,
            boolean isTransient) {
        this.notificationId = notificationId;
        this.isOffTheRecord = isOffTheRecord;
        this.canDownloadWhileMetered = canDownloadWhileMetered;
        this.fileName = fileName;
        this.isAutoResumable = isAutoResumable;
        this.id = id != null ? id : new ContentId();
        this.isTransient = isTransient;
    }

    /**
     * Parse the pending notification from a String object in SharedPrefs.
     *
     * @param sharedPrefString String from SharedPreference, containing the notification ID, GUID,
     *        file name, whether it is resumable and whether download started on a metered network.
     * @return a DownloadSharedPreferenceEntry object.
     */
    static DownloadSharedPreferenceEntry parseFromString(String sharedPrefString) {
        int version = -1;
        try {
            String versionString = sharedPrefString.substring(0, sharedPrefString.indexOf(","));
            version = Integer.parseInt(versionString);
        } catch (NumberFormatException ex) {
            Log.w(TAG, "Exception while parsing pending download:" + sharedPrefString);
            return INVALID_ENTRY;
        }

        switch (version) {
            case 1:
                return parseFromVersion1(sharedPrefString);
            case 2:
                return parseFromVersion2(sharedPrefString);
            case 3:
                return parseFromVersion3(sharedPrefString);
            case 4:
                return parseFromVersion4(sharedPrefString);
            case 5:
                return parseFromVersion5(sharedPrefString);
            case 6:
                return parseFromVersion6(sharedPrefString);
            default:
                return INVALID_ENTRY;
        }
    }

    static DownloadSharedPreferenceEntry parseFromVersion1(String string) {
        String[] entries = string.split(",", 6);
        if (entries.length != 6) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,ONTHERECORD,METERED,GUID,FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringOnTheRecord = entries[2];
        String stringMetered = entries[3];
        String stringGuid = entries[4];
        String stringFileName = entries[5];

        boolean onTheRecord = "1".equals(stringOnTheRecord);
        boolean metered = "1".equals(stringMetered);
        int version;
        int notificationId;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 1) return INVALID_ENTRY;
        if (!isValidGUID(stringGuid)) return INVALID_ENTRY;

        return new DownloadSharedPreferenceEntry(
                LegacyHelpers.buildLegacyContentId(false, stringGuid), notificationId, !onTheRecord,
                metered, stringFileName, true, false);
    }

    static DownloadSharedPreferenceEntry parseFromVersion2(String string) {
        String[] entries = string.split(",", 6);
        if (entries.length != 6) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,OFFTHERECORD,METERED,GUID,FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringOffTheRecord = entries[2];
        String stringMetered = entries[3];
        String stringGuid = entries[4];
        String stringFileName = entries[5];

        boolean offTheRecord = "1".equals(stringOffTheRecord);
        boolean metered = "1".equals(stringMetered);
        int version;
        int notificationId;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 2) return INVALID_ENTRY;
        if (!isValidGUID(stringGuid)) return INVALID_ENTRY;

        return new DownloadSharedPreferenceEntry(
                LegacyHelpers.buildLegacyContentId(false, stringGuid), notificationId, offTheRecord,
                metered, stringFileName, true, false);
    }

    static DownloadSharedPreferenceEntry parseFromVersion3(String string) {
        final int itemTypeDownload = 1;
        final int itemTypeOfflinePage = 2;

        String[] entries = string.split(",", 7);
        if (entries.length != 7) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,ITEMTYPE,OFFTHERECORD,METERED,GUID,FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringItemType = entries[2];
        String stringOffTheRecord = entries[3];
        String stringMetered = entries[4];
        String stringGuid = entries[5];
        String stringFileName = entries[6];

        boolean offTheRecord = "1".equals(stringOffTheRecord);
        boolean metered = "1".equals(stringMetered);
        int version;
        int notificationId;
        int itemType;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
            itemType = Integer.parseInt(stringItemType);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 3) return INVALID_ENTRY;
        if (!isValidGUID(stringGuid)) return INVALID_ENTRY;
        if (itemType != itemTypeDownload && itemType != itemTypeOfflinePage) {
            return INVALID_ENTRY;
        }

        boolean isOfflinePage = itemType == itemTypeOfflinePage;

        return new DownloadSharedPreferenceEntry(
                LegacyHelpers.buildLegacyContentId(isOfflinePage, stringGuid), notificationId,
                offTheRecord, metered, stringFileName, true, false);
    }

    static DownloadSharedPreferenceEntry parseFromVersion4(String string) {
        final int itemTypeDownload = 1;
        final int itemTypeOfflinePage = 2;

        String[] entries = string.split(",", 8);
        if (entries.length != 8) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,TYPE,OFFTHERECORD,METEREDOK,AUTORESUMEOK,GUID,FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringItemType = entries[2];
        String stringOffTheRecord = entries[3];
        String stringMetered = entries[4];
        String stringAutoResume = entries[5];
        String stringGuid = entries[6];
        String stringFileName = entries[7];

        boolean offTheRecord = "1".equals(stringOffTheRecord);
        boolean metered = "1".equals(stringMetered);
        boolean autoResume = "1".equals(stringAutoResume);
        int version;
        int notificationId;
        int itemType;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
            itemType = Integer.parseInt(stringItemType);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 4) return INVALID_ENTRY;
        if (!isValidGUID(stringGuid)) return INVALID_ENTRY;
        if (itemType != itemTypeDownload && itemType != itemTypeOfflinePage) {
            return INVALID_ENTRY;
        }

        boolean isOfflinePage = itemType == itemTypeOfflinePage;

        return new DownloadSharedPreferenceEntry(
                LegacyHelpers.buildLegacyContentId(isOfflinePage, stringGuid), notificationId,
                offTheRecord, metered, stringFileName, autoResume, false);
    }

    static DownloadSharedPreferenceEntry parseFromVersion5(String string) {
        String[] entries = string.split(",", 8);
        if (entries.length != 8) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,NAMESPACE,GUID,OFFTHERECORD,METEREDOK,AUTORESUMEOK,FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringNamespace = entries[2];
        String stringGuid = entries[3];
        String stringOffTheRecord = entries[4];
        String stringMetered = entries[5];
        String stringAutoResume = entries[6];
        String stringFileName = entries[7];

        boolean offTheRecord = "1".equals(stringOffTheRecord);
        boolean metered = "1".equals(stringMetered);
        boolean autoResume = "1".equals(stringAutoResume);
        int version;
        int notificationId;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 5) return INVALID_ENTRY;
        if (!isValidGUID(stringGuid)) return INVALID_ENTRY;
        if (TextUtils.isEmpty(stringNamespace)) return INVALID_ENTRY;

        return new DownloadSharedPreferenceEntry(new ContentId(stringNamespace, stringGuid),
                notificationId, offTheRecord, metered, stringFileName, autoResume, false);
    }

    static DownloadSharedPreferenceEntry parseFromVersion6(String string) {
        String[] entries = string.split(",", 9);
        if (entries.length != 9) return INVALID_ENTRY;
        // VERSION,NOTIFICATIONID,NAMESPACE,ID,OFFTHERECORD,METEREDOK,AUTORESUMEOK,ISTRANSIENT,
        // FILENAME
        String stringVersion = entries[0];
        String stringNotificationId = entries[1];
        String stringNamespace = entries[2];
        String stringId = entries[3];
        String stringOffTheRecord = entries[4];
        String stringMetered = entries[5];
        String stringAutoResume = entries[6];
        String stringTransient = entries[7];
        String stringFileName = entries[8];

        boolean offTheRecord = "1".equals(stringOffTheRecord);
        boolean metered = "1".equals(stringMetered);
        boolean autoResume = "1".equals(stringAutoResume);
        boolean isTransient = "1".equals(stringTransient);
        int version;
        int notificationId;
        try {
            version = Integer.parseInt(stringVersion);
            notificationId = Integer.parseInt(stringNotificationId);
        } catch (NumberFormatException ex) {
            return INVALID_ENTRY;
        }

        if (version != 6) return INVALID_ENTRY;
        if (TextUtils.isEmpty(stringId)) return INVALID_ENTRY;
        if (TextUtils.isEmpty(stringNamespace)) return INVALID_ENTRY;

        return new DownloadSharedPreferenceEntry(new ContentId(stringNamespace, stringId),
                notificationId, offTheRecord, metered, stringFileName, autoResume, isTransient);
    }

    /**
     * @return a string for the DownloadSharedPreferenceEntry instance to be inserted into
     *         SharedPrefs.
     */
    String getSharedPreferenceString() {
        String serialized = "";
        serialized += VERSION + ",";
        serialized += notificationId + ",";
        serialized += id.namespace + ",";
        serialized += id.id + ",";
        serialized += (isOffTheRecord ? "1" : "0") + ",";
        serialized += (canDownloadWhileMetered ? "1" : "0") + ",";
        serialized += (isAutoResumable ? "1" : "0") + ",";
        serialized += (isTransient ? "1" : "0") + ",";
        // Keep filename as the last serialized entry because a filename can have commas in it.
        serialized += fileName;

        return serialized;
    }

    /**
     * Check if a string is a valid GUID. GUID is RFC 4122 compliant, it should have format
     * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.
     * TODO(qinmin): move this to base/.
     * @return true if the string is a valid GUID, or false otherwise.
     */
    static boolean isValidGUID(String guid) {
        if (guid == null) return false;
        try {
            // Java UUID class doesn't check the length of the string. Need to convert it back to
            // string so that we can validate the length of the original string.
            UUID uuid = UUID.fromString(guid);
            String uuidString = uuid.toString();
            return guid.equalsIgnoreCase(uuidString);
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    /**
     * Build a download item from this object.
     */
    DownloadItem buildDownloadItem() {
        DownloadInfo info = new DownloadInfo.Builder()
                                    .setDownloadGuid(id.id)
                                    .setIsOfflinePage(LegacyHelpers.isLegacyOfflinePage(id))
                                    .setFileName(fileName)
                                    .setIsOffTheRecord(isOffTheRecord)
                                    .setBytesReceived(DownloadManagerService.UNKNOWN_BYTES_RECEIVED)
                                    .setContentId(id)
                                    .setIsTransient(isTransient)
                                    .build();
        return new DownloadItem(false, info);
    }

    @Override
    public boolean equals(Object object) {
        if (!(object instanceof DownloadSharedPreferenceEntry)) {
            return false;
        }
        final DownloadSharedPreferenceEntry other = (DownloadSharedPreferenceEntry) object;
        return id.equals(other.id) && TextUtils.equals(fileName, other.fileName)
                && notificationId == other.notificationId && isOffTheRecord == other.isOffTheRecord
                && canDownloadWhileMetered == other.canDownloadWhileMetered
                && isAutoResumable == other.isAutoResumable && isTransient == other.isTransient;
    }

    @Override
    public int hashCode() {
        int hash = 31;
        hash = 37 * hash + (isOffTheRecord ? 1 : 0);
        hash = 37 * hash + (canDownloadWhileMetered ? 1 : 0);
        hash = 37 * hash + (isAutoResumable ? 1 : 0);
        hash = 37 * hash + notificationId;
        hash = 37 * hash + id.hashCode();
        hash = 37 * hash + fileName.hashCode();
        hash = 37 * hash + (isTransient ? 1 : 0);
        return hash;
    }
}
