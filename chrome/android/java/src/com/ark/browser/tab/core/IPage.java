package com.ark.browser.tab.core;

import com.ark.browser.tab.PageInfo;

import org.chromium.chrome.browser.tab.Tab;

public interface IPage {

    int getId();

    public PageInfo getPageInfo();

    void remove();

    Tab getNativePage();

//    /**
//     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
//     *
//     * @param observer The {@link TabObserver} to add.
//     */
//    public void addObserver(TabObserver observer) {
//        mObservers.addObserver(observer);
//    }

}
