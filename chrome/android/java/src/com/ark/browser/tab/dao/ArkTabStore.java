package com.ark.browser.tab.dao;

import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.List;

public class ArkTabStore {

    private static final String TAG = "ArkTabStore";

    private static final int SAVED_STATE_VERSION = 5;

    private final Deque<Tab> mTabsToSave = new ArrayDeque<>();

    private final SequencedTaskRunner mSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);

    private SaveTabTask mSaveTabTask;
//    private SaveListTask mSaveListTask;

    private boolean mDestroyed;

//    private byte[] mLastSavedMetadata;


    public void addTabToSaveQueue(Tab tab) {
        addTabToSaveQueueIfApplicable(tab);
        saveNextTab();
    }

    private void addTabToSaveQueueIfApplicable(Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        if (mTabsToSave.contains(tab) || !TabStateAttributes.from(tab).isTabStateDirty()
                || isTabUrlContentScheme(tab)) {
            return;
        }

        if (UrlUtilities.isNTPUrl(tab.getUrl()) && !tab.canGoBack() && !tab.canGoForward()) {
            return;
        }
        mTabsToSave.addLast(tab);
    }

    private boolean isTabUrlContentScheme(Tab tab) {
        GURL url = tab.getUrl();
        return url != null && url.getScheme().equals(UrlConstants.CONTENT_SCHEME);
    }

    void saveNextTab() {
        if (mSaveTabTask != null) return;
        if (!mTabsToSave.isEmpty()) {
            Tab tab = mTabsToSave.removeFirst();
            mSaveTabTask = new SaveTabTask(tab);
            mSaveTabTask.executeOnTaskRunner(mSequencedTaskRunner);
        } else {
            saveTabListAsynchronously();
        }
    }

    public void saveTabListAsynchronously() {
//        if (mSaveListTask != null) mSaveListTask.cancel(true);
//        mSaveListTask = new SaveListTask();
//        mSaveListTask.executeOnTaskRunner(mSequencedTaskRunner);
    }

    private boolean saveTabState(int tabId, boolean encrypted, TabState state) {
        if (state == null) return false;

        try {
            TabStateFileManager.saveState(ArkTabDao.getTabStateFile(tabId, encrypted), state, encrypted);
            return true;
        } catch (OutOfMemoryError e) {
            ArkLogger.e(
                    TAG, "Out of memory error while attempting to save tab state.  Erasing.");
            deleteTabState(tabId, encrypted);
        }
        return false;
    }

//    private void saveListToFile(byte[] listData) {
//        if (Arrays.equals(mLastSavedMetadata, listData)) return;
//
//        saveListToFile(getStateDirectory(), mPersistencePolicy.getStateFileName(), listData);
//        mLastSavedMetadata = listData;
//        if (LibraryLoader.getInstance().isInitialized()) {
//            RecordHistogram.recordCount1MHistogram(
//                    "Android.TabPersistentStore.MetadataFileSize", listData.length);
//        }
//    }

    private void deleteTabState(int id, boolean encrypted) {
        File file = ArkTabDao.getTabStateFile(id, encrypted);
        if (file.exists() && !file.delete()) {
            ArkLogger.e(TAG, "Failed to delete TabState: %s", file);
        }
    }


    private static boolean isCriticalPersistedTabDataEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA);
    }



    public static TabPersistentStore.TabModelSelectorMetadata serializeTabModelSelector(TabModelSelector selector) throws IOException {
        ThreadUtils.assertOnUiThread();

        TabModel incognitoModel = selector.getModel(true);
        // TODO(crbug/783819): Convert TabModelMetadata to use GURL.
        TabPersistentStore.TabModelMetadata incognitoInfo = new TabPersistentStore.TabModelMetadata(incognitoModel.index());
        for (int i = 0; i < incognitoModel.getCount(); i++) {
            incognitoInfo.ids.add(incognitoModel.getTabAt(i).getId());
            incognitoInfo.urls.add(incognitoModel.getTabAt(i).getUrl().getSpec());
        }

        TabModel normalModel = selector.getModel(false);
        TabPersistentStore.TabModelMetadata normalInfo = new TabPersistentStore.TabModelMetadata(normalModel.index());
        for (int i = 0; i < normalModel.getCount(); i++) {
            normalInfo.ids.add(normalModel.getTabAt(i).getId());
            normalInfo.urls.add(normalModel.getTabAt(i).getUrl().getSpec());
        }

        // Cache the active tab id to be pre-loaded next launch.
        int activeTabId = Tab.INVALID_TAB_ID;
        int activeIndex = normalModel.index();
        @TabPersistentStore.ActiveTabState
        int activeTabState = TabPersistentStore.ActiveTabState.EMPTY;
        if (activeIndex != TabList.INVALID_TAB_INDEX) {
            Tab activeTab = normalModel.getTabAt(activeIndex);
            activeTabId = activeTab.getId();
            activeTabState = UrlUtilities.isNTPUrl(activeTab.getUrl()) ? TabPersistentStore.ActiveTabState.NTP
                    : TabPersistentStore.ActiveTabState.OTHER;
        }

        byte[] listData = serializeMetadata(normalInfo, incognitoInfo);
        return new TabPersistentStore.TabModelSelectorMetadata(listData, normalInfo, incognitoInfo);
    }


    public static byte[] serializeMetadata(
            TabPersistentStore.TabModelMetadata standardInfo, TabPersistentStore.TabModelMetadata incognitoInfo) throws IOException {
        int standardCount = standardInfo.ids.size();
        int incognitoCount = incognitoInfo.ids.size();

        // Determine how many Tabs there are.
        int numTabsTotal = incognitoCount + standardCount;

        // Save the index file containing the list of tabs to restore.
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);
        stream.writeInt(SAVED_STATE_VERSION);
        stream.writeInt(numTabsTotal);
        stream.writeInt(incognitoCount);
        stream.writeInt(incognitoInfo.index);
        stream.writeInt(standardInfo.index + incognitoCount);
        Log.i(TAG, "Serializing tab lists; counts: " + standardCount + ", " + incognitoCount);

        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.REGULAR_TAB_COUNT, standardCount);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.INCOGNITO_TAB_COUNT, incognitoCount);

        // Save incognito state first, so when we load, if the incognito files are unreadable
        // we can fall back easily onto the standard selected tab.
        for (int i = 0; i < incognitoCount; i++) {
            stream.writeInt(incognitoInfo.ids.get(i));
            stream.writeUTF(incognitoInfo.urls.get(i));
        }
        for (int i = 0; i < standardCount; i++) {
            stream.writeInt(standardInfo.ids.get(i));
            stream.writeUTF(standardInfo.urls.get(i));
        }

        stream.close();
        return output.toByteArray();
    }



//    protected static final class TabRestoreDetails {
//        public final int id;
//        public final int originalIndex;
//        public final String url;
//        public final Boolean isIncognito;
//        public final Boolean fromMerge;
//
//        public TabRestoreDetails(int id, int originalIndex, Boolean isIncognito, String url,
//                                 Boolean fromMerge) {
//            this.id = id;
//            this.originalIndex = originalIndex;
//            this.url = url;
//            this.isIncognito = isIncognito;
//            this.fromMerge = fromMerge;
//        }
//    }
//
//    public static class TabModelSelectorMetadata {
//        public final byte[] listData;
//        public final TabPersistentStore.TabModelMetadata normalModelMetadata;
//        public final TabPersistentStore.TabModelMetadata incognitoModelMetadata;
//
//        public TabModelSelectorMetadata(byte[] listData, TabPersistentStore.TabModelMetadata normalModelMetadata,
//                                        TabPersistentStore.TabModelMetadata incognitoModelMetadata) {
//            this.listData = listData;
//            this.normalModelMetadata = normalModelMetadata;
//            this.incognitoModelMetadata = incognitoModelMetadata;
//        }
//    }


    private class SaveTabTask extends AsyncTask<Void> {
        Tab mTab;
        int mId;
        TabState mState;
        boolean mEncrypted;
        boolean mStateSaved;

        SaveTabTask(Tab tab) {
            mTab = tab;
            mId = tab.getId();
            mEncrypted = tab.isIncognito();
        }

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            mState = TabStateExtractor.from(mTab);
        }

        @Override
        protected Void doInBackground() {
            mStateSaved = saveTabState(mId, mEncrypted, mState);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;
            if (mStateSaved) {
                if (!mTab.isDestroyed()) TabStateAttributes.from(mTab).setIsTabStateDirty(false);
                mTab.setIsTabSaveEnabled(isCriticalPersistedTabDataEnabled());
//                migrateSomeRemainingTabsToCriticalPersistedTabData();
            }
            mSaveTabTask = null;
            saveNextTab();
        }
    }

//    private class SaveListTask extends AsyncTask<Void> {
//        TabPersistentStore.TabModelSelectorMetadata mMetadata;
//
//        @Override
//        protected void onPreExecute() {
//            if (mDestroyed || isCancelled()) return;
//            try {
//                mMetadata = serializeTabMetadata();
//            } catch (IOException e) {
//                mMetadata = null;
//            }
//        }
//
//        @Override
//        protected Void doInBackground() {
//            if (mMetadata == null || isCancelled()) return null;
//            saveListToFile(mMetadata.listData);
//            return null;
//        }
//
//        @Override
//        protected void onPostExecute(Void v) {
//            if (mDestroyed || isCancelled()) {
//                mMetadata = null;
//                return;
//            }
//
//            if (mSaveListTask == this) {
//                mSaveListTask = null;
//                for (TabPersistentStore.TabPersistentStoreObserver observer : mObservers) {
//                    observer.onMetadataSavedAsynchronously(mMetadata);
//                }
//                mMetadata = null;
//            }
//        }
//    }

}
