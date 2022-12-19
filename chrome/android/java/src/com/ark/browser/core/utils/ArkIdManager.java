package com.ark.browser.core.utils;

import com.zpj.utils.PrefsHelper;

import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.atomic.AtomicInteger;

public class ArkIdManager {

    private final AtomicInteger mIdCounter = new AtomicInteger();

    private final PrefsHelper mPreferences;


    private ArkIdManager(String prefsName) {
        mPreferences = PrefsHelper.with(prefsName);
        mIdCounter.set(mPreferences.getInt("next_id", 0));
    }


    private int generateId(int id) {
        if (id == Tab.INVALID_TAB_ID) id = mIdCounter.getAndIncrement();
        incrementIdCounterTo(id + 1);
        return id;
    }

    private void incrementIdCounterTo(int id) {
        int diff = id - mIdCounter.get();
        if (diff < 0) return;

        int num = mIdCounter.addAndGet(diff);
        mPreferences.applyInt("next_id", num);
    }


    private static class Holder {
        private static final ArkIdManager TAB_ID_MANAGER = new ArkIdManager("tab_id_manager");
    }

    private static class PageHolder {
        private static final ArkIdManager PAGE_ID_MANAGER = new ArkIdManager("page_id_manager");
    }


    public static int generateTabId(int id) {
        return Holder.TAB_ID_MANAGER.generateId(id);
    }

    public static int generateTabId() {
        return generateTabId(Tab.INVALID_TAB_ID);
    }

    public static int generatePageId(int id) {
        return PageHolder.PAGE_ID_MANAGER.generateId(id);
    }

    public static int generatePageId() {
        return generatePageId(Tab.INVALID_PAGE_ID);
    }

}