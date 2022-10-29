package com.ark.browser.tab.core;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabSnapshotManager;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ThreadPool;

import org.chromium.chrome.browser.tab.Tab;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

public interface IPage {

    int getId();

    PageInfo getPageInfo();

    default void remove() {
        PageCacheManager.getInstance().removePage(getPageInfo());
        TabSnapshotManager.getInstance().removeSnapshot(getId());
        deletePageInfo();
    }

    Tab getNativePage();

//    /**
//     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
//     *
//     * @param observer The {@link TabObserver} to add.
//     */
//    public void addObserver(TabObserver observer) {
//        mObservers.addObserver(observer);
//    }

    default void deletePageInfo() {
        File pagesDir = ArkTabDao.getPagesDir(getPageInfo().getTabInfoId());
        File file = new File(pagesDir, String.valueOf(getId()));
        file.delete();
    }

    default void savePageInfo() {
        ThreadPool.executeIO(() -> {
            PageInfo pageInfo = getPageInfo();
            File pagesDir = ArkTabDao.getPagesDir(pageInfo.tabInfoId);
            File file = new File(pagesDir, String.valueOf(pageInfo.pageId));
            try (DataOutputStream os = new DataOutputStream(
                    new BufferedOutputStream(new FileOutputStream(file)))) {
                int version = 1;
                os.writeInt(version);
                os.writeInt(pageInfo.pageId);
                os.writeLong(pageInfo.tabInfoId);
                os.writeBoolean(pageInfo.isIncognito);
                os.writeBoolean(pageInfo.fromMerge);
                os.writeInt(pageInfo.getThemeColor());
                os.writeInt(pageInfo.originalIndex);
                os.writeUTF(pageInfo.getUrl());
                os.writeUTF(pageInfo.getTitle());
                os.flush();
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }

}
