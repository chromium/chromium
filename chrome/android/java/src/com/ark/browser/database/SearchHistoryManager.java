package com.ark.browser.database;

import androidx.annotation.WorkerThread;
import androidx.core.util.AtomicFile;

import com.ark.browser.model.SearchHistory;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.zpj.utils.ContextUtils;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

public class SearchHistoryManager {

    static final String NAME = "search_history_manager";
    public static final int VERSION = 1;

    private static final class Holder {

        private static final LinkedList<String> SEARCH_HISTORY_LIST = new LinkedList<>();

        static {
            try (DataInputStream is = ArkTabDao.readFile(
                    new File(ContextUtils.getApplicationContext().getFilesDir(), NAME))) {
                if (is != null) {
                    int version = is.readInt();
                    int size = is.readInt();
                    for (int i = 0; i < size; i++) {
                        SEARCH_HISTORY_LIST.add(is.readUTF());
                    }
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

    }

    @WorkerThread
    public static List<SearchHistory> getAllSearchHistory() {
        return getSearchHistoryLimited(0);
    }

    @WorkerThread
    public static List<SearchHistory> getSearchHistoryLimited(int limit) {
        List<String> historyList;
        if (limit > 0) {
            limit = Math.min(limit, Holder.SEARCH_HISTORY_LIST.size());
            historyList = Holder.SEARCH_HISTORY_LIST.subList(0, limit);
        } else {
            historyList = Holder.SEARCH_HISTORY_LIST;
        }

        List<SearchHistory> list = new ArrayList<>();
        for (String history : historyList) {
            SearchHistory searchHistory = new SearchHistory();
            searchHistory.setText(history);
            searchHistory.setTime(System.currentTimeMillis());
            list.add(searchHistory);
        }
        return list;
    }

    @WorkerThread
    public static SearchHistory getSearchHistoryByText(String target) {
        if (Holder.SEARCH_HISTORY_LIST.contains(target)) {
            SearchHistory searchHistory = new SearchHistory();
            searchHistory.setTime(System.currentTimeMillis());
            searchHistory.setText(target);
            return searchHistory;
        }
        return null;
    }

    @WorkerThread
    public static void deleteAllLocalSearchHistory() {
        Holder.SEARCH_HISTORY_LIST.clear();
        AtomicFile file = new AtomicFile(new File(ContextUtils.getApplicationContext().getFilesDir(), NAME));
        file.delete();
    }

    @WorkerThread
    public static void deleteSearchHistoryByText(String target) {
        Holder.SEARCH_HISTORY_LIST.remove(target);
        save();
    }

    @WorkerThread
    public static void saveSearchHistory(String history) {
        Holder.SEARCH_HISTORY_LIST.remove(history);
        Holder.SEARCH_HISTORY_LIST.addFirst(history);
        save();
    }

    private static void save() {
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);

            int version = 1;
            os.writeInt(version);
            os.writeInt(Holder.SEARCH_HISTORY_LIST.size());
            for (int i = 0; i < Holder.SEARCH_HISTORY_LIST.size(); i++) {
                os.writeUTF(Holder.SEARCH_HISTORY_LIST.get(i));
            }
            os.close();

            byte[] bytes = stream.toByteArray();


            AtomicFile file = new AtomicFile(new File(ContextUtils.getApplicationContext().getFilesDir(), NAME));
            FileOutputStream fos = null;
            try {
                fos = file.startWrite();
                fos.write(bytes, 0, bytes.length);
                file.finishWrite(fos);
            } catch (IOException e) {
                if (fos != null) file.failWrite(fos);
                ArkLogger.e(SearchHistoryManager.class,
                        "Failed to write file: " + file.getBaseFile().getAbsolutePath());
            }
        } catch (Exception e) {

        }
    }

}
