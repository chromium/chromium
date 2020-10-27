// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * Class for saving most visited sites info.
 *
 * This class is a singleton, since at any point at most one MostVisitedSitesHost can exist for
 * ensuring that saving files is atomic.
 */
public class MostVisitedSitesHost implements MostVisitedSites.Observer {
    private static final String TAG = "TopSites";

    // The map mapping URL to faviconId. This map will be updated once there are new suggestions
    // available.
    private final Map<GURL, Integer> mUrlToIdMap = new HashMap<>();

    // The map mapping faviconId to URL. This map will be reconstructed based on the mUrlToIdMap
    // once there are new suggestions available.
    private final Map<Integer, GURL> mIdToUrlMap = new HashMap<>();

    // The set of URLs needed to fetch favicon. This set will be reconstructed once there are new
    // suggestions available.
    private final Set<GURL> mUrlsToUpdateFavicon = new HashSet<>();

    private static boolean sSkipRestoreFromDiskForTests;
    /** The singleton helper class for this class. */
    private static class SingletonHelper {
        private static final MostVisitedSitesHost INSTANCE = new MostVisitedSitesHost();
    }

    private MostVisitedSites mMostVisitedSites;
    private MostVisitedSitesFaviconHelper mFaviconSaver;
    private Runnable mCurrentTask;
    private Runnable mPendingTask;
    // Whether restoreFromDisk() is finished.
    private boolean mIsSynced;
    // Records how many remaining files the current task needs to save.
    private int mCurrentFilesNeedToSaveCount;
    // Records how many remaining files the pending task needs to save. This value is used to
    // replace mCurrentFilesNeedToSaveCount when updating pending to current task.
    private int mPendingFilesNeedToSaveCount;

    private MostVisitedSitesHost() {
        if (!sSkipRestoreFromDiskForTests) {
            restoreFromDisk();
        }
    }

    /**
     * @return The singleton instance.
     */
    public static MostVisitedSitesHost getInstance() {
        return SingletonHelper.INSTANCE;
    }

    /**
     * Once new siteSuggestions info is available, call this function to update map and set and save
     * data to the disk. If syncing with disk hasn't finished or there is a current task running,
     * make this new task the pending task. Otherwise, make this new task the current task and run
     * it.
     * @param siteSuggestions The new SiteSuggestions.
     */
    public void saveMostVisitedSitesInfo(List<SiteSuggestion> siteSuggestions) {
        if (mFaviconSaver == null) {
            LargeIconBridge largeIconBridge =
                    SuggestionsDependencyFactory.getInstance().createLargeIconBridge(
                            Profile.getLastUsedRegularProfile());
            mFaviconSaver = new MostVisitedSitesFaviconHelper(
                    ContextUtils.getApplicationContext(), largeIconBridge);
        }

        // Ensure that saving happens after map and set are updated. Use finishOneFileSaving() as
        // callback to record when this current task has been finished.
        Runnable newTask = () -> updateMapAndSetForNewSites(siteSuggestions, () -> {
            MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                    siteSuggestions, this::finishOneFileSaving);
            mFaviconSaver.saveFaviconsToFile(
                    siteSuggestions, mUrlsToUpdateFavicon, this::finishOneFileSaving);
        });

        if (!mIsSynced || mCurrentTask != null) {
            // Skip last mPendingTask which is not necessary to run.
            mPendingTask = newTask;
            mPendingFilesNeedToSaveCount = siteSuggestions.size() + 1;
        } else {
            // Assign newTask to mCurrentTask and run this task.
            mCurrentTask = newTask;
            mCurrentFilesNeedToSaveCount = siteSuggestions.size() + 1;
            // Skip any pending task.
            mPendingTask = null;
            mPendingFilesNeedToSaveCount = 0;

            Log.d(TAG, "Start a new task.");
            mCurrentTask.run();
        }
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        saveMostVisitedSitesInfo(siteSuggestions);
    }

    @Override
    public void onIconMadeAvailable(GURL siteUrl) {}

    /**
     * Start the observer.
     * @param maxResults The max number of sites to observe.
     */
    public void startObserving(int maxResults) {
        if (mMostVisitedSites == null) {
            mMostVisitedSites = SuggestionsDependencyFactory.getInstance().createMostVisitedSites(
                    Profile.getLastUsedRegularProfile());
        }
        mMostVisitedSites.setObserver(this, maxResults);
    }

    /**
     * Restore disk info to the mUrlToIDMap.
     */
    private void restoreFromDisk() {
        new AsyncTask<List<SiteSuggestion>>() {
            @Override
            protected List<SiteSuggestion> doInBackground() {
                List<SiteSuggestion> siteSuggestions = new ArrayList<>();
                try {
                    siteSuggestions = MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();
                } catch (IOException e) {
                    Log.i(TAG, "No top sites lists file existed in the disk.");
                }
                return siteSuggestions;
            }

            @Override
            protected void onPostExecute(List<SiteSuggestion> siteSuggestions) {
                mUrlToIdMap.clear();
                mIdToUrlMap.clear();
                for (SiteSuggestion site : siteSuggestions) {
                    mUrlToIdMap.put(site.url, site.faviconId);
                }
                buildIdToUrlMap();
                mIsSynced = true;
                updatePendingToCurrent();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Update mUrlToIDMap and mUrlsToUpdateFavicon based on the new SiteSuggestions passed in.
     * @param newSuggestions The new SiteSuggestions.
     * @param callback The callback function after updating map and set.
     */
    @VisibleForTesting
    protected void updateMapAndSetForNewSites(
            List<SiteSuggestion> newSuggestions, Runnable callback) {
        new AsyncTask<Set<String>>() {
            @Override
            protected Set<String> doInBackground() {
                return getExistingIconFiles();
            }

            @Override
            protected void onPostExecute(Set<String> existingIconFiles) {
                // Clear mUrlsToUpdateFavicon.
                mUrlsToUpdateFavicon.clear();
                // Update mIdToUrlMap with current mUrlToIdMap.
                buildIdToUrlMap();

                // Update mUrlsToUpdateFavicon based on the new SiteSuggestions.
                refreshUrlsToUpdate(newSuggestions, existingIconFiles);

                // Update mUrlToIdMap based on the new SiteSuggestions.
                updateMapForNewSites(newSuggestions, callback);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private static Set<String> getExistingIconFiles() {
        Set<String> existingIconFiles = new HashSet<>();
        File topSitesDirectory = MostVisitedSitesMetadataUtils.getStateDirectory();

        if (topSitesDirectory == null || topSitesDirectory.list() == null) {
            return existingIconFiles;
        }

        existingIconFiles.addAll(Arrays.asList(Objects.requireNonNull(topSitesDirectory.list())));
        return existingIconFiles;
    }

    /**
     * Update mUrlsToUpdateFavicon based on the new SiteSuggestions passed in.
     * @param newSuggestions The new SiteSuggestions.
     * @param existingIconFiles The existing favicon files name set.
     */
    private void refreshUrlsToUpdate(
            List<SiteSuggestion> newSuggestions, Set<String> existingIconFiles) {
        // Add topsites URLs which need to fetch icon to the mUrlsToUpdateFavicon Set.
        for (SiteSuggestion topSiteData : newSuggestions) {
            GURL url = topSiteData.url;
            // If the old map doesn't contain the URL or there is no favicon file for this URL, then
            // add this URL to mUrlsToUpdateFavicon.
            if (!mUrlToIdMap.containsKey(url)
                    || !existingIconFiles.contains(String.valueOf(mUrlToIdMap.get(url)))) {
                mUrlsToUpdateFavicon.add(url);
            }
        }
    }

    /**
     * Update the mUrlToIdMap based on the new SiteSuggestions and mUrlsToUpdateFavicon.
     * @param newSuggestions The new SiteSuggestions.
     * @param callback The callback function after updating map and set.
     */
    private void updateMapForNewSites(List<SiteSuggestion> newSuggestions, Runnable callback) {
        // Get the set of new top sites' URLs.
        Set<GURL> newUrls = new HashSet<>();
        for (SiteSuggestion topSiteData : newSuggestions) {
            newUrls.add(topSiteData.url);
        }

        // Add new URLs and ids to the mUrlToIDMap.
        int id = 0;
        for (GURL url : mUrlsToUpdateFavicon) {
            if (mUrlToIdMap.containsKey(url)) {
                continue;
            }
            // Get the next available ID.
            id = getNextAvailableId(id, newUrls);
            mUrlToIdMap.put(url, id);
            id++;
        }

        // Remove stale data in mUrlToIdMap.
        List<Integer> idsToDeleteFile = removeStaleData(newUrls);

        // Remove stale favicon files in the disk asynchronously.
        deleteStaleFilesAsync(idsToDeleteFile, () -> {
            mIdToUrlMap.clear();

            // Save faviconIDs to newSuggestions.
            for (SiteSuggestion siteData : newSuggestions) {
                siteData.faviconId = Objects.requireNonNull(mUrlToIdMap.get(siteData.url));
            }

            if (callback != null) {
                callback.run();
            }
        });
    }

    @VisibleForTesting
    protected int getNextAvailableId(int start, Set<GURL> newTopSiteUrls) {
        int id = start;
        // The available ids should be in range [0, newTopSiteUrls.size()), since we only need
        // |newTopSiteUrls.size()| ids.
        for (; id < newTopSiteUrls.size(); id++) {
            // If this id is not used in mUrlToIdMap or URL corresponding to this id is not in
            // the newTopSiteURLs, it's available.
            if (!mIdToUrlMap.containsKey(id) || !newTopSiteUrls.contains(mIdToUrlMap.get(id))) {
                return id;
            }
        }
        assert false : "Unreachable code.";
        return id;
    }

    @VisibleForTesting
    protected void buildIdToUrlMap() {
        mIdToUrlMap.clear();
        for (Map.Entry<GURL, Integer> entry : mUrlToIdMap.entrySet()) {
            mIdToUrlMap.put(entry.getValue(), entry.getKey());
        }
    }

    /**
     * Return faviconIds which need to delete files.
     * @param newUrls The URLs in new SiteSuggestions.
     * @return The list of faviconIds needed to remove.
     */
    private List<Integer> removeStaleData(Set<GURL> newUrls) {
        List<Integer> idsToDeleteFile = new ArrayList<>();
        for (Iterator<Map.Entry<GURL, Integer>> it = mUrlToIdMap.entrySet().iterator();
                it.hasNext();) {
            Map.Entry<GURL, Integer> entry = it.next();
            GURL url = entry.getKey();
            int faviconId = entry.getValue();
            if (!newUrls.contains(url)) {
                it.remove();
                idsToDeleteFile.add(faviconId);
            }
        }
        return idsToDeleteFile;
    }

    private void deleteStaleFilesAsync(List<Integer> idsToDeleteFile, Runnable callback) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                for (int id : idsToDeleteFile) {
                    File file =
                            new File(MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory(),
                                    String.valueOf(id));
                    file.delete();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (callback != null) {
                    callback.run();
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    protected void updatePendingToCurrent() {
        mCurrentTask = mPendingTask;
        mCurrentFilesNeedToSaveCount = mPendingFilesNeedToSaveCount;
        mPendingTask = null;
        if (mCurrentTask != null) {
            Log.d(TAG, "Start a new task.");
            mCurrentTask.run();
        }
    }

    private void finishOneFileSaving() {
        ThreadUtils.assertOnUiThread();
        mCurrentFilesNeedToSaveCount--;

        // If there is no file needed to save for current task, update pending task to current.
        if (mCurrentFilesNeedToSaveCount == 0) {
            updatePendingToCurrent();
        }
    }

    @VisibleForTesting
    protected Map<GURL, Integer> getUrlToIDMapForTesting() {
        return mUrlToIdMap;
    }

    @VisibleForTesting
    protected Set<GURL> getUrlsToUpdateFaviconForTesting() {
        return mUrlsToUpdateFavicon;
    }

    @VisibleForTesting
    protected Map<Integer, GURL> getIdToUrlMapForTesting() {
        return mIdToUrlMap;
    }

    @VisibleForTesting
    protected static void setSkipRestoreFromDiskForTesting() {
        sSkipRestoreFromDiskForTests = true;
    }

    @VisibleForTesting
    public void setIsSyncedForTesting(boolean isSynced) {
        mIsSynced = isSynced;
    }

    @VisibleForTesting
    public int getCurrentFilesNeedToSaveCountForTesting() {
        return mCurrentFilesNeedToSaveCount;
    }

    @VisibleForTesting
    public int getPendingFilesNeedToSaveCountForTesting() {
        return mPendingFilesNeedToSaveCount;
    }

    @VisibleForTesting
    public void setCurrentTaskForTesting(Runnable currentTask) {
        mCurrentTask = currentTask;
    }

    @VisibleForTesting
    public void setPendingTaskForTesting(Runnable pendingTask) {
        mPendingTask = pendingTask;
    }
}
