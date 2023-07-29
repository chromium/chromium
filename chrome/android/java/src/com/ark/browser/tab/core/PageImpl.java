package com.ark.browser.tab.core;

import androidx.annotation.NonNull;

import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabCacheManager;

import org.chromium.chrome.browser.tab.Tab;

public class PageImpl implements IPage {

    @NonNull
    private final PageInfo pageInfo;

    public PageImpl(@NonNull PageInfo pageInfo) {
        this.pageInfo = pageInfo;
    }

    @Override
    public int getId() {
        return pageInfo.getId();
    }

    @NonNull
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
