package com.ark.browser.tab.core;

import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.PageInfo;

import org.chromium.chrome.browser.tab.Tab;

public class PageImpl implements IPage {

    private final PageInfo pageInfo;

    public PageImpl(PageInfo pageInfo) {
        this.pageInfo = pageInfo;
    }

    @Override
    public int getId() {
        return pageInfo.getId();
    }

    @Override
    public PageInfo getPageInfo() {
        return pageInfo;
    }

//    @Override
//    public void remove() {
//        PageCacheManager.getInstance().removePage(pageInfo);
//        TabSnapshotManager.getInstance().removeSnapshot(pageInfo.getPageId());
//        pageInfo.deleteSync();
//    }

    @Override
    public Tab getNativePage() {
        return TabCacheManager.getInstance().findTab(getId());
    }

    @Override
    public String toString() {
        return "PageImpl{" +
                "pageInfo=" + pageInfo +
                '}';
    }
}
