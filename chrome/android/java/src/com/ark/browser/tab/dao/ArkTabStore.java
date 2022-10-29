package com.ark.browser.tab.dao;

import com.ark.browser.utils.ArkLogger;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.io.File;
import java.util.ArrayDeque;
import java.util.Deque;

public class ArkTabStore {

    private static final String TAG = "ArkTabStore";

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
