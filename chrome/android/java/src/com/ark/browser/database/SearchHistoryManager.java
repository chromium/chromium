package com.ark.browser.database;

import com.ark.browser.model.SearchHistory;

import java.util.ArrayList;
import java.util.List;

public class SearchHistoryManager {

    static final String NAME = "search_history_manager";
    public static final int VERSION = 1;

    public static List<SearchHistory> getAllSearchHistory() {
        return new ArrayList<>();
    }

    public static List<SearchHistory> getSearchHistoryLimited() {
        return new ArrayList<>();
    }

    public static SearchHistory getSearchHistoryByText(String text) {
        return null;
    }

    public static void deleteAllLocalSearchHistory() {

    }

    public static void deleteSearchHistoryByText(String text) {

    }

}
