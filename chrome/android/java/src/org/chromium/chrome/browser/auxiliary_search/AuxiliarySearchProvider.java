// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.PersistableBundle;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.url.GURL;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** This class provides information for the auxiliary search. */
public class AuxiliarySearchProvider {
    /** The callback interface to get results from fetching a favicon. * */
    public interface FaviconImageFetchedCallback {
        /** This method will be called when the result favicon is ready. */
        void onFaviconAvailable(Bitmap image, AuxiliarySearchEntry entry);
    }

    /* Only donate the recent 7 days accessed tabs.*/
    @VisibleForTesting static final String TAB_AGE_HOURS_PARAM = "tabs_max_hours";
    @VisibleForTesting static final String TASK_CREATED_TIME = "TaskCreatedTime";
    @VisibleForTesting static final String TAB_DONATE_FILE_NAME = "tabs_donate";
    @VisibleForTesting static final int DEFAULT_TAB_AGE_HOURS = 168;
    @VisibleForTesting static final int DEFAULT_FAVICON_NUMBER = 5;

    /** The current version of the saved Tab donate metadata file. */
    private static final int SAVED_STATE_VERSION = 1;

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

    private static final String MAX_FAVICON_NUMBER_PARAM = "max_favicon_number";
    public static final IntCachedFieldTrialParameter MAX_FAVICON_NUMBER =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    MAX_FAVICON_NUMBER_PARAM,
                    DEFAULT_FAVICON_NUMBER);

    private final Context mContext;
    private final Profile mProfile;
    private final AuxiliarySearchBridge mAuxiliarySearchBridge;
    private final @Nullable TabModelSelector mTabModelSelector;
    private final @NonNull FaviconHelper mFaviconHelper;
    private final int mDefaultFaviconSize;
    private final boolean mIsFaviconEnabled;
    private final int mMaxFaviconNumber;
    private Long mTabMaxAgeMillis;

    public AuxiliarySearchProvider(
            @NonNull Context context,
            @NonNull Profile profile,
            @Nullable TabModelSelector tabModelSelector) {
        mContext = context;
        mProfile = profile;
        mAuxiliarySearchBridge = new AuxiliarySearchBridge(mProfile);
        mTabModelSelector = tabModelSelector;
        mTabMaxAgeMillis = getTabsMaxAgeMs();
        mFaviconHelper = new FaviconHelper();
        mDefaultFaviconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        mIsFaviconEnabled = ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();
        mMaxFaviconNumber = MAX_FAVICON_NUMBER.getValue();
    }

    /**
     * @return AuxiliarySearchGroup for bookmarks.
     */
    public AuxiliarySearchBookmarkGroup getBookmarksSearchableDataProto() {
        return mAuxiliarySearchBridge.getBookmarksSearchableData();
    }

    /**
     * @param callback {@link Callback} to pass back the AuxiliarySearchGroup for {@link Tab}s with
     *     favicons.
     * @param faviconImageFetchedCallback The callback to be called when the fetching of favicon is
     *     complete.
     */
    public void getTabsSearchableDataProtoWithFaviconAsync(
            @NonNull Callback<AuxiliarySearchTabGroup> callback,
            @Nullable FaviconImageFetchedCallback faviconImageFetchedCallback) {
        long startTimeMs = System.currentTimeMillis();
        long minAccessTime = startTimeMs - mTabMaxAgeMillis;
        List<Tab> listTab = getTabsByMinimalAccessTime(minAccessTime);

        mAuxiliarySearchBridge.getNonSensitiveTabs(
                listTab,
                tabs -> {
                    onNonSensitiveTabsAvailable(
                            mFaviconHelper,
                            callback,
                            faviconImageFetchedCallback,
                            tabs,
                            startTimeMs);
                });
    }

    @VisibleForTesting
    void onNonSensitiveTabsAvailable(
            @NonNull FaviconHelper faviconHelper,
            @NonNull Callback<AuxiliarySearchTabGroup> callback,
            @Nullable FaviconImageFetchedCallback faviconImageFetchedCallback,
            @NonNull List<Tab> tabs,
            long starTimeMs) {
        var tabGroupBuilder = AuxiliarySearchTabGroup.newBuilder();

        int count = 0;
        if (mIsFaviconEnabled) {
            tabs.sort(sComparator);
        }

        for (Tab tab : tabs) {
            AuxiliarySearchEntry entry = tabToAuxiliarySearchEntry(tab);
            if (entry != null) {
                tabGroupBuilder.addTab(entry);
                if (!mIsFaviconEnabled || count >= mMaxFaviconNumber) continue;

                count++;
                faviconHelper.getLocalFaviconImageForURL(
                        mProfile,
                        tab.getUrl(),
                        mDefaultFaviconSize,
                        (image, url) -> {
                            if (faviconImageFetchedCallback != null) {
                                faviconImageFetchedCallback.onFaviconAvailable(image, entry);
                            }
                        });
            }
        }

        if (mIsFaviconEnabled && mMaxFaviconNumber < tabs.size()) {
            saveTabMetadataToFile(getTabDonateFile(mContext), tabs, mMaxFaviconNumber);
        }
        callback.onResult(tabGroupBuilder.build());

        if (mIsFaviconEnabled) {
            scheduleBackgroundTask(0L, starTimeMs);
        }
    }

    @VisibleForTesting
    static @Nullable AuxiliarySearchEntry tabToAuxiliarySearchEntry(@Nullable Tab tab) {
        if (tab == null) {
            return null;
        }

        String title = tab.getTitle();
        GURL url = tab.getUrl();
        if (url == null || !url.isValid()) return null;

        return createAuxiliarySearchEntry(
                tab.getId(), title, url.getSpec(), tab.getTimestampMillis());
    }

    @VisibleForTesting
    static @Nullable AuxiliarySearchEntry createAuxiliarySearchEntry(
            int id, @NonNull String title, @NonNull String url, long timestamp) {
        if (TextUtils.isEmpty(title) || url == null) return null;

        var tabBuilder = AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url).setId(id);
        if (timestamp != Tab.INVALID_TIMESTAMP) {
            tabBuilder.setLastAccessTimestamp(timestamp);
        }
        return tabBuilder.build();
    }

    /** Returns the file to save the metadata for donating tabs. */
    static File getTabDonateFile(Context context) {
        return new File(context.getFilesDir(), TAB_DONATE_FILE_NAME);
    }

    /**
     * Saves the tabs' metadata to a file.
     *
     * @param metadataFile The file to write.
     * @param tabs A list of tabs to save.
     * @param startIndex The index of the first tabs to save.
     */
    void saveTabMetadataToFile(
            @NonNull File metadataFile, @NonNull List<Tab> tabs, int startIndex) {
        synchronized (SAVE_LIST_LOCK) {
            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream output = null;
            try {
                output = file.startWrite();
                int size = tabs.size();

                DataOutputStream stream = new DataOutputStream(new BufferedOutputStream(output));
                stream.writeInt(SAVED_STATE_VERSION);
                stream.writeInt(size - mMaxFaviconNumber);

                for (int i = startIndex; i < size; i++) {
                    Tab tab = tabs.get(i);
                    stream.writeInt(tab.getId());
                    stream.writeUTF(tab.getTitle());
                    stream.writeUTF(tab.getUrl().getSpec());
                    stream.writeLong(tab.getTimestampMillis());
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
     */
    @Nullable
    static List<AuxiliarySearchEntry> readSavedMetadataFile(@Nullable DataInputStream stream)
            throws IOException {
        if (stream == null) return null;

        final int version = stream.readInt();
        assert version == SAVED_STATE_VERSION;

        final int count = stream.readInt();
        if (count < 0) {
            return null;
        }

        List<AuxiliarySearchEntry> entryList = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            int id = stream.readInt();
            String title = stream.readUTF();
            String url = stream.readUTF();
            long timeStamp = stream.readLong();
            AuxiliarySearchEntry entry = createAuxiliarySearchEntry(id, title, url, timeStamp);
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
    @NonNull
    List<Tab> getTabsByMinimalAccessTime(long minAccessTime) {
        TabList allTabs = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> recentAccessedTabs = new ArrayList<>();

        for (int i = 0; i < allTabs.getCount(); i++) {
            Tab tab = allTabs.getTabAt(i);
            if (tab.getTimestampMillis() >= minAccessTime) {
                recentAccessedTabs.add(allTabs.getTabAt(i));
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
        if (!mIsFaviconEnabled) return null;

        PersistableBundle bundle = new PersistableBundle();
        bundle.putLong(TASK_CREATED_TIME, startTimeMs);

        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        TaskInfo.TimingInfo oneOffTimingInfo =
                TaskInfo.OneOffInfo.create().setWindowStartTimeMs(windowStartTimeMs).build();

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
}
