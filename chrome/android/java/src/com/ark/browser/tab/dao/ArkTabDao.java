package com.ark.browser.tab.dao;

import android.content.Context;
import android.util.SparseBooleanArray;

import androidx.annotation.Nullable;

import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.tab.core.TabGroupImpl;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

public class ArkTabDao {

    private static final String TAG = "ArkTabDao";

    private static final String DIR_GROUPS = "groups";
    private static final String DIR_TABS = "tabs";
    private static final String DIR_PAGES = "pages";

    private static class StateDirHolder {

        private static final File sDirectory;

        static {
            sDirectory = ContextUtils.getApplicationContext().getDir(
                    "tab_list", Context.MODE_PRIVATE);
        }
    }

    public static ITabGroup loadTabGroup(ArkWindowAndroid nativeWindow, boolean incognito) {
        int id = incognito ? 1 : 0;
        File groupFile = new File(getGroupsDir(), "group_" + id);
        if (groupFile.exists()) {
            return new TabGroupImpl(nativeWindow, incognito, groupFile);
        } else {
            return new TabGroupImpl(nativeWindow, incognito);
        }
    }

    public static File getGroupsDir() {
        File groupsDir = new File(StateDirHolder.sDirectory, DIR_GROUPS);
        if (!groupsDir.exists()) {
            groupsDir.mkdirs();
        }
        return groupsDir;
    }

    public static File getTabFile(long id) {
        File tabsDir = new File(StateDirHolder.sDirectory, DIR_TABS);
        if (!tabsDir.exists()) {
            tabsDir.mkdirs();
        }
        return new File(tabsDir, "tab" + id);
    }

    public static File getPagesDir(long tabInfoId) {
        File tabsDir = new File(StateDirHolder.sDirectory, DIR_PAGES);
        File dir = new File(tabsDir, String.valueOf(tabInfoId));
        if (!dir.exists()) {
            dir.mkdirs();
        }
        return dir;
    }

//    public static File getPageFile(int int pageId) {
//
//    }

    public static DataInputStream readFile(File file) {
        Log.i(TAG, "Starting to fetch tab list for " + file);
        if (!file.exists()) {
            return null;
        }
        FileInputStream stream = null;
        byte[] data;
        try {
            stream = new FileInputStream(file);
            data = new byte[(int) file.length()];
            stream.read(data);
        } catch (IOException exception) {
            Log.e(TAG, "Could not read state file.", exception);
            return null;
        } finally {
            StreamUtil.closeQuietly(stream);
        }
        Log.i(TAG, "Finished fetching tab list.");
        return new DataInputStream(new ByteArrayInputStream(data));
    }

    public static AsyncTask<DataInputStream> fetchGroupFile(final File groupFile) {
        return new BackgroundOnlyAsyncTask<DataInputStream>() {
            @Override
            protected DataInputStream doInBackground() {
                return readFile(groupFile);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public static int readSavedStateFile(DataInputStream stream, @Nullable SparseBooleanArray tabIds)
            throws IOException {
        if (stream == null) return 0;
        int nextId = 0;
//        boolean skipUrlRead = false;
//        boolean skipIncognitoCount = false;
        final int version = stream.readInt();

        final int count = stream.readInt();
        final int index = stream.readInt();
        final boolean isIncognito = stream.readBoolean();

        for (int i = 0; i < count; i++) {
            int tabId = stream.readInt();
            if (tabIds != null) tabIds.append(tabId, true);
        }

        return nextId;
    }


    public static TabState restorePageState(int pageId) {
        File pageFile = getTabStateFile(pageId, false);
        return TabStateFileManager.restoreTabState(pageFile, false);
    }

    public static void savePageState(Tab page) {
        boolean encrypted = page.isIncognito();
        File pageFile = getTabStateFile(page.getId(), encrypted);
        TabStateFileManager.saveState(pageFile, TabStateExtractor.from(page), encrypted);
    }

    public static File getTabStateFile(int tabId, boolean encrypted) {

        File statesDir= new File(StateDirHolder.sDirectory, "states");
        if (!statesDir.exists()) {
            statesDir.mkdirs();
        }

        return new File(statesDir, "page" + tabId);
    }



}
