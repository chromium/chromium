// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.os.PersistableBundle;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchHostType;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.url.GURL;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** This class provides information for the auxiliary search. */
@NullMarked
public class AuxiliarySearchProvider {
    /** The version of tab donation's metadata. */
    @IntDef({MetaDataVersion.V1, MetaDataVersion.MULTI_TYPE_V2, MetaDataVersion.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface MetaDataVersion {
        int V1 = 0;
        int MULTI_TYPE_V2 = 1;
        int NUM_ENTRIES = 2;
    }

    /* Only donate the recent 7 days accessed tabs.*/
    @VisibleForTesting static final String TAB_AGE_HOURS_PARAM = "tabs_max_hours";
    @VisibleForTesting static final String TASK_CREATED_TIME = "TaskCreatedTime";
    @VisibleForTesting static final int DEFAULT_TAB_AGE_HOURS = 168;

    @VisibleForTesting
    static final int DEFAULT_WINDOW_END_TIME_MS = 60 * 1000; // 1 min in milliseconds.

    /** Prevents two AuxiliarySearchProvider from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    /**
     * A comparator to sort Tabs with timestamp descending, i.e., the most recent tab comes first.
     */
    @VisibleForTesting
    static Comparator<Tab> sComparator =
            (tab1, tab2) -> {
                long delta = tab1.getTimestampMillis() - tab2.getTimestampMillis();
                return (int) -Math.signum((float) delta);
            };

    private final Context mContext;
    private final Profile mProfile;
    private final @Nullable TabModelSelector mTabModelSelector;

    private final Long mTabMaxAgeMillis;
    @Nullable private AuxiliarySearchBridge mAuxiliarySearchBridge;

    public AuxiliarySearchProvider(
            Context context,
            Profile profile,
            @Nullable TabModelSelector tabModelSelector,
            @AuxiliarySearchHostType int hostType) {
        mContext = context;
        mProfile = profile;
        if (hostType != AuxiliarySearchHostType.BACKGROUND_TASK) {
            mAuxiliarySearchBridge = new AuxiliarySearchBridge(mProfile);
        }
        mTabModelSelector = tabModelSelector;
        mTabMaxAgeMillis = getTabsMaxAgeMs();
    }

    /** Returns a list of non sensitive Tabs. */
    public void getTabsSearchableDataProtoAsync(Callback<@Nullable List<Tab>> callback) {
        if (mAuxiliarySearchBridge == null) {
            callback.onResult(null);
            return;
        }

        long minAccessTime = System.currentTimeMillis() - mTabMaxAgeMillis;
        List<Tab> listTab = getTabsByMinimalAccessTime(minAccessTime);

        // We will get up to 100 tabs as default. This is controlled by feature
        // AuxiliarySearchDonation.
        mAuxiliarySearchBridge.getNonSensitiveTabs(listTab, callback);
    }

    /** Returns a list of non sensitive data from supported data types. */
    public void getHistorySearchableDataProtoAsync(
            Callback<@Nullable List<AuxiliarySearchDataEntry>> callback) {
        if (mAuxiliarySearchBridge == null) {
            callback.onResult(null);
            return;
        }
        // We will get up to 100 tabs as default. This is controlled by feature
        // AuxiliarySearchDonation.
        mAuxiliarySearchBridge.getNonSensitiveHistoryData(callback);
    }

    public void getCustomTabsAsync(
            GURL url, long beginTime, Callback<@Nullable List<AuxiliarySearchDataEntry>> callback) {
        if (mAuxiliarySearchBridge == null) {
            callback.onResult(null);
            return;
        }

        mAuxiliarySearchBridge.getCustomTabs(url, beginTime, callback);
    }

    @VisibleForTesting
    static @Nullable AuxiliarySearchEntry createAuxiliarySearchEntry(
            int id, String title, String url, long timestamp) {
        if (TextUtils.isEmpty(title) || url == null) return null;

        var tabBuilder = AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url).setId(id);
        if (timestamp != Tab.INVALID_TIMESTAMP) {
            tabBuilder.setLastAccessTimestamp(timestamp);
        }
        return tabBuilder.build();
    }

    /**
     * Saves metadata to a file.
     *
     * @param metadataFile The file to write.
     * @param entries A list of data to save.
     * @param startIndex The index of the first entry to save.
     * @param entryCountToSave The count of entries to save to the file. This is the count of the
     *     remaining entries which haven't been donated yet.
     * @param <T> The type of the entry data for donation.
     */
    <T> void saveTabMetadataToFile(
            File metadataFile, int version, List<T> entries, int startIndex, int entryCountToSave) {
        synchronized (SAVE_LIST_LOCK) {
            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream output = null;
            try {
                output = file.startWrite();

                DataOutputStream stream = new DataOutputStream(new BufferedOutputStream(output));
                stream.writeInt(version);
                stream.writeInt(entryCountToSave);

                for (int i = 0; i < entryCountToSave; i++) {
                    T entry = entries.get(i + startIndex);
                    if (entry instanceof Tab tab) {
                        assert version == MetaDataVersion.V1;
                        stream.writeInt(tab.getId());
                        stream.writeUTF(tab.getTitle());
                        stream.writeUTF(tab.getUrl().getSpec());
                        stream.writeLong(tab.getTimestampMillis());
                    } else if (entry instanceof AuxiliarySearchDataEntry dataEntry) {
                        assert version == MetaDataVersion.MULTI_TYPE_V2;
                        @AuxiliarySearchEntryType int type = dataEntry.type;
                        stream.writeInt(type);
                        if (type == AuxiliarySearchEntryType.TAB) {
                            stream.writeInt(dataEntry.tabId);
                        } else {
                            if (type == AuxiliarySearchEntryType.CUSTOM_TAB) {
                                stream.writeUTF(dataEntry.appId);
                            } else if (type == AuxiliarySearchEntryType.TOP_SITE) {
                                stream.writeInt(dataEntry.score);
                            }
                            stream.writeInt(dataEntry.visitId);
                        }
                        stream.writeUTF(dataEntry.title);
                        stream.writeUTF(dataEntry.url.getSpec());
                        stream.writeLong(dataEntry.lastActiveTime);
                    }
                }

                stream.flush();
                file.finishWrite(output);
            } catch (IOException e) {
                if (output != null) file.failWrite(output);
            }
        }
    }

    /**
     * Extracts the tab information from a given tab donation metadata stream.
     *
     * @param stream The stream pointing to the tab donation metadata file to be parsed.
     * @param <T> The type of the entry data for donation.
     */
    static <T> @Nullable List<T> readSavedMetadataFile(@Nullable DataInputStream stream)
            throws IOException {
        if (stream == null) return null;

        final int version = stream.readInt();
        final int count = stream.readInt();
        if (count < 0) {
            return null;
        }

        List<T> entryList = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            T entry = null;
            if (version == MetaDataVersion.V1) {
                int id = stream.readInt();
                String title = stream.readUTF();
                String url = stream.readUTF();
                long timeStamp = stream.readLong();
                entry = (T) createAuxiliarySearchEntry(id, title, url, timeStamp);
            } else if (version == MetaDataVersion.MULTI_TYPE_V2) {
                int type = stream.readInt();
                int id = Tab.INVALID_TAB_ID;
                String appId = null;
                int visitId = Tab.INVALID_TAB_ID;
                int score = -1;
                if (type == AuxiliarySearchEntryType.TAB) {
                    id = stream.readInt();
                } else {
                    if (type == AuxiliarySearchEntryType.CUSTOM_TAB) {
                        appId = stream.readUTF();
                    } else if (type == AuxiliarySearchEntryType.TOP_SITE) {
                        score = stream.readInt();
                    }
                    visitId = stream.readInt();
                }
                String title = stream.readUTF();
                String url = stream.readUTF();
                long timeStamp = stream.readLong();
                entry =
                        (T)
                                new AuxiliarySearchDataEntry(
                                        type,
                                        new GURL(url),
                                        title,
                                        timeStamp,
                                        id,
                                        appId,
                                        visitId,
                                        score);
            }
            if (entry != null) {
                entryList.add(entry);
            }
        }

        return entryList;
    }

    /**
     * @param minAccessTime specifies the earliest access time for a tab to be included in the
     *     returned list.
     * @return List of {@link Tab} which is accessed after 'minAccessTime'.
     */
    @VisibleForTesting
    List<Tab> getTabsByMinimalAccessTime(long minAccessTime) {
        if (mTabModelSelector == null) return Collections.emptyList();

        TabList allTabs = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> recentAccessedTabs = new ArrayList<>();

        for (int i = 0; i < allTabs.getCount(); i++) {
            Tab tab = allTabs.getTabAtChecked(i);
            if (tab.getTimestampMillis() >= minAccessTime) {
                recentAccessedTabs.add(tab);
            }
        }

        return recentAccessedTabs;
    }

    /** Returns the donated tab's max age in MS. */
    @VisibleForTesting
    long getTabsMaxAgeMs() {
        int configuredTabMaxAgeHrs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.ANDROID_APP_INTEGRATION, TAB_AGE_HOURS_PARAM, 0);
        if (configuredTabMaxAgeHrs == 0) configuredTabMaxAgeHrs = DEFAULT_TAB_AGE_HOURS;
        return TimeUnit.HOURS.toMillis(configuredTabMaxAgeHrs);
    }

    /**
     * Schedule a {@link AuxiliarySearchBackgroundTask} for donating more favicons.
     *
     * @param windowStartTimeMs The delay to schedule a background task.
     * @param startTimeMs The start time when the task is created but not scheduled.
     */
    @VisibleForTesting
    TaskInfo scheduleBackgroundTask(long windowStartTimeMs, long startTimeMs) {
        assert ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();

        PersistableBundle bundle = new PersistableBundle();
        bundle.putLong(TASK_CREATED_TIME, startTimeMs);

        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        TaskInfo.TimingInfo oneOffTimingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(windowStartTimeMs)
                        .setWindowEndTimeMs(DEFAULT_WINDOW_END_TIME_MS)
                        .build();

        TaskInfo.Builder builder =
                TaskInfo.createTask(TaskIds.AUXILIARY_SEARCH_DONATE_JOB_ID, oneOffTimingInfo);
        builder.setUserInitiated(false)
                .setUpdateCurrent(true)
                .setIsPersisted(true)
                .setExtras(bundle);

        TaskInfo taskInfo = builder.build();
        scheduler.schedule(mContext, taskInfo);
        return taskInfo;
    }

    public boolean isAuxiliarySearchBridgeNullForTesting() {
        return mAuxiliarySearchBridge == null;
    }
}
