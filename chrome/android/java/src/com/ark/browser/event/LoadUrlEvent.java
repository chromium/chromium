package com.ark.browser.event;

import com.ark.browser.tab.PageInfo;
import com.zpj.bus.ZBus;

public class LoadUrlEvent {

    private final String url;
    private final boolean isNewTab;
    private final boolean incognito;
    private final PageInfo pageInfo;

    private LoadUrlEvent(String url, boolean isNewTab, boolean incognito) {
        this(null, url, isNewTab, incognito);
    }

    private LoadUrlEvent(PageInfo pageInfo, String url, boolean isNewTab, boolean incognito) {
        this.url = url;
        this.isNewTab = isNewTab;
        this.incognito = incognito;
        this.pageInfo = pageInfo;
    }

    public String getUrl() {
        return url;
    }

    public boolean isNewTab() {
        return isNewTab;
    }

    public boolean isIncognito() {
        return incognito;
    }

    public PageInfo getPageInfo() {
        return pageInfo;
    }

    public void post() {
        ZBus.post(this);
    }

    public static void post(String url) {
        post(url, false);
    }

    public static void post(String url, boolean isNewTab) {
        post(url, isNewTab, false);
    }

    public static void post(String url, boolean isNewTab, boolean incognito) {
        new LoadUrlEvent(url, isNewTab, incognito).post();
    }

    public static void post(PageInfo pageInfo, boolean isNewTab, boolean incognito) {
        if (pageInfo == null) {
            return;
        }
        post(pageInfo, pageInfo.getUrl(), isNewTab, incognito);
    }

    public static void post(PageInfo pageInfo, String url, boolean isNewTab, boolean incognito) {
        new LoadUrlEvent(pageInfo, url, isNewTab, incognito).post();
    }

}

