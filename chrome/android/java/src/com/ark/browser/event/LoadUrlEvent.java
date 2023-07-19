package com.ark.browser.event;

import com.ark.browser.tab.PageInfo;
import com.zpj.bus.ZBus;

import org.chromium.content_public.browser.LoadUrlParams;

public class LoadUrlEvent {

    private final LoadUrlParams loadUrlParams;
    private final boolean isNewTab;
    private final boolean incognito;
    private final PageInfo pageInfo;

    private LoadUrlEvent(LoadUrlParams params, boolean isNewTab, boolean incognito) {
        this(null, params, isNewTab, incognito);
    }

    private LoadUrlEvent(PageInfo pageInfo, LoadUrlParams params, boolean isNewTab, boolean incognito) {
        loadUrlParams = params;
        this.isNewTab = isNewTab;
        this.incognito = incognito;
        this.pageInfo = pageInfo;
    }

//    public String getUrl() {
//        return loadUrlParams.getUrl();
//    }

    public LoadUrlParams getLoadUrlParams() {
        return loadUrlParams;
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
        post(new LoadUrlParams(url), isNewTab, incognito);
    }

    public static void post(LoadUrlParams loadUrlParams, boolean isNewTab) {
        post(loadUrlParams, isNewTab, false);
    }

    public static void post(LoadUrlParams loadUrlParams, boolean isNewTab, boolean incognito) {
        new LoadUrlEvent(loadUrlParams, isNewTab, incognito).post();
    }

    public static void post(PageInfo pageInfo, boolean isNewTab, boolean incognito) {
        if (pageInfo == null) {
            return;
        }
        post(pageInfo, pageInfo.getUrl(), isNewTab, incognito);
    }

    public static void post(PageInfo pageInfo, String url, boolean isNewTab, boolean incognito) {
        new LoadUrlEvent(pageInfo, new LoadUrlParams(url), isNewTab, incognito).post();
    }

    public static void post(PageInfo pageInfo, LoadUrlParams loadUrlParams, boolean isNewTab, boolean incognito) {
        new LoadUrlEvent(pageInfo, loadUrlParams, isNewTab, incognito).post();
    }

}

