package com.ark.browser.tab.core;

import androidx.annotation.NonNull;
import androidx.core.util.AtomicFile;

import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

public interface IPage {

    int getId();

    PageInfo getPageInfo();

    default IPage clone(int newTabId) {
        return new PageImpl(getPageInfo().clonePageInfo(newTabId));
    }

    default void remove() {
        ArkWebManager.remove(getId());
        PageSnapshotManager.getInstance().removeSnapshot(getId());
        ThreadPool.executeIO(this::deletePageInfo);
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
        File pagesDir = ArkTabDao.getPagesDir(getPageInfo().getTabId());
        File file = new File(pagesDir, String.valueOf(getId()));
        file.delete();
    }

    default void loadUrl(LoadUrlParams params) {

    }

    default void savePageInfo() {

        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);

            PageInfo pageInfo = getPageInfo();
            int version = 1;
            os.writeInt(version);
            os.writeInt(pageInfo.pageId);
            os.writeInt(pageInfo.tabId);
            os.writeBoolean(pageInfo.isIncognito);
            os.writeBoolean(pageInfo.fromMerge);
            os.writeInt(pageInfo.getThemeColor());
            os.writeInt(pageInfo.originalIndex);
            os.writeUTF(pageInfo.getUrl());
            os.writeUTF(pageInfo.getTitle());
            os.close();

            byte[] bytes = stream.toByteArray();

            ThreadPool.executeIO(new Runnable() {
                @Override
                public void run() {
                    File pagesDir = ArkTabDao.getPagesDir(pageInfo.tabId);
                    AtomicFile file = new AtomicFile(new File(pagesDir, String.valueOf(pageInfo.pageId)));
                    ArkLogger.e(this, "savePageInfo pagesDir=" + pagesDir);
                    FileOutputStream fos = null;
                    try {
                        fos = file.startWrite();
                        fos.write(bytes, 0, bytes.length);
                        file.finishWrite(fos);
                        ArkLogger.e(this, "savePageInfo success!");
                    } catch (IOException e) {
                        if (fos != null) file.failWrite(fos);
                        ArkLogger.e(this, "savePageInfo Failed to write file: " + file.getBaseFile().getAbsolutePath());
                    }
                }
            });

        } catch (IOException e) {
            e.printStackTrace();
        }



//        ThreadPool.executeIO(() -> {
//            PageInfo pageInfo = getPageInfo();
//            File pagesDir = ArkTabDao.getPagesDir(pageInfo.tabInfoId);
//            File file = new File(pagesDir, String.valueOf(pageInfo.pageId));
//            try (DataOutputStream os = new DataOutputStream(
//                    new BufferedOutputStream(new FileOutputStream(file)))) {
//                int version = 1;
//                os.writeInt(version);
//                os.writeInt(pageInfo.pageId);
//                os.writeInt(pageInfo.tabInfoId);
//                os.writeBoolean(pageInfo.isIncognito);
//                os.writeBoolean(pageInfo.fromMerge);
//                os.writeInt(pageInfo.getThemeColor());
//                os.writeInt(pageInfo.originalIndex);
//                os.writeUTF(pageInfo.getUrl());
//                os.writeUTF(pageInfo.getTitle());
//                os.flush();
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//        });
    }

}
