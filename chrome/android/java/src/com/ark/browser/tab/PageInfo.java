package com.ark.browser.tab;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.core.util.AtomicFile;

import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.chrome.browser.tab.Tab;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

@Keep
//@Table(database = PageInfoManager.class)
public class PageInfo {

//    @PrimaryKey()
    public int pageId;

//    @Column(defaultValue = "-1")
    public int parentId;

//    @Column
    public long tabInfoId;
//    @Column
    public int originalIndex;
//    @Column
    public boolean isIncognito;
//    @Column
    public boolean fromMerge;

//    @Column(defaultValue = "-1")
    private int themeColor;

//    @Column
    private String url;

//    @Column
    private String title;

    private PageInfo() {

    }

//    @Override
//    public void save() {
//        ThreadPool.executeIO(() -> {
//            File pagesDir = ArkTabDao.getPagesDir(tabInfoId);
//            File file = new File(pagesDir, String.valueOf(pageId));
//            try (DataOutputStream os = new DataOutputStream(
//                    new BufferedOutputStream(new FileOutputStream(file)))) {
//                int version = 1;
//                os.writeInt(version);
//                os.writeInt(pageId);
//                os.writeLong(tabInfoId);
//                os.writeBoolean(isIncognito);
//                os.writeBoolean(fromMerge);
//                os.writeInt(themeColor);
//                os.writeInt(originalIndex);
//                os.writeUTF(url);
//                os.writeUTF(title);
//                os.flush();
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//        });
//    }
//
//    @Override
//    public void deleteSync() {
//        File pagesDir = ArkTabDao.getPagesDir(tabInfoId);
//        File file = new File(pagesDir, String.valueOf(pageId));
//        file.delete();
//    }

    public static PageInfo from(int pageId, int parentId, long tabInfoId, int index, boolean isIncognito) {
        PageInfo info = new PageInfo();
        info.pageId = pageId;
        info.parentId = parentId;
        info.originalIndex = index;
        info.tabInfoId = tabInfoId;
        info.isIncognito = isIncognito;
        info.fromMerge = false;
        info.save();
        return info;
    }

//    public static PageInfo from(Tab tab, String tabInfoId, int index) {
//        PageInfo info = new PageInfo();
//        info.pageId = tab.getId();
//        info.originalIndex = index;
//        info.tabInfoId = tabInfoId;
//        info.isIncognito = tab.isIncognito();
//        info.fromMerge = false;
//        info.themeColor = tab.getThemeColor();
//        info.url = tab.getUrl();
//        info.title = tab.getTitle();
//        return info;
//    }

//    public static PageInfo from(String tabInfoId, PageInfo pageInfo) {
//        PageInfo info = new PageInfo();
//        info.pageId = pageInfo.pageId;
//        info.originalIndex = pageInfo.originalIndex;
//        info.tabInfoId = tabInfoId;
//        info.isIncognito = pageInfo.isIncognito;
//        info.fromMerge = pageInfo.fromMerge;
//        info.themeColor = pageInfo.themeColor;
//        info.url = pageInfo.url;
//        info.title = pageInfo.title;
//        return info;
//    }

    public static PageInfo from(PageInfo pageInfo) {
        PageInfo info = new PageInfo();
        info.pageId = pageInfo.pageId;
        info.originalIndex = pageInfo.originalIndex;
        info.tabInfoId = pageInfo.tabInfoId;
        info.isIncognito = pageInfo.isIncognito;
        info.fromMerge = pageInfo.fromMerge;
        info.themeColor = pageInfo.themeColor;
        info.url = pageInfo.url;
        info.title = pageInfo.title;
        info.save();
        return info;
    }


    public static PageInfo from(File pageFile) throws IOException {
        try (DataInputStream stream = ArkTabDao.readFile(pageFile)) {
            if (stream == null) {
                throw new IOException("page file stream is null!");
            }
            return from(stream);
        }
//        DataInputStream stream = ArkTabDao.readFile(pageFile);
//        if (stream == null) {
//            throw new IOException("page file stream is null!");
//        }
//        return from(stream);
    }

    public static PageInfo from(DataInputStream is) throws IOException {
        PageInfo info = new PageInfo();
        int version = is.readInt();
        info.pageId = is.readInt();
        info.tabInfoId = is.readLong();
        info.isIncognito = is.readBoolean();
        info.fromMerge = is.readBoolean();
        info.themeColor = is.readInt();
        info.originalIndex = is.readInt();
        info.url = is.readUTF();
        info.title = is.readUTF();
        return info;
    }

    public int getPageId() {
        return pageId;
    }

    public void setPageId(int pageId) {
        this.pageId = pageId;
    }

    public int getParentId() {
        return parentId;
    }

    public void setParentId(int parentId) {
        this.parentId = parentId;
    }

    public long getTabInfoId() {
        return tabInfoId;
    }

    public void setTabInfoId(long tabInfoId) {
        this.tabInfoId = tabInfoId;
    }

    public int getOriginalIndex() {
        return originalIndex;
    }

    public void setOriginalIndex(int originalIndex) {
        this.originalIndex = originalIndex;
    }

    public Boolean isIncognito() {
        return isIncognito;
    }

    public void setIncognito(Boolean incognito) {
        isIncognito = incognito;
    }

    public Boolean getFromMerge() {
        return fromMerge;
    }

    public void setFromMerge(Boolean fromMerge) {
        this.fromMerge = fromMerge;
    }

    public int getThemeColor() {
        return themeColor;
    }

    public void setThemeColor(int themeColor) {
        this.themeColor = themeColor;
    }

    public String getUrl() {
        return url == null ? "" : url;
    }

    public void setUrl(String url) {
        if (TextUtils.equals(this.url, url)) {
            return;
        }
        this.url = url;
        save();
    }

    public String getTitle() {
        return title == null ? getUrl() : title;
    }

    public void setTitle(String title) {
        if (TextUtils.equals(this.title, title)) {
            return;
        }
        this.title = title;
        save();
    }

    @Override
    public String toString() {
        return "PageInfo{" +
                "pageId=" + pageId +
                ", parentId=" + parentId +
                ", tabInfoId='" + tabInfoId + '\'' +
                ", originalIndex=" + originalIndex +
                ", isIncognito=" + isIncognito +
                ", fromMerge=" + fromMerge +
                ", themeColor=" + themeColor +
                ", url='" + url + '\'' +
                ", title='" + title + '\'' +
                '}';
    }

    private void save() {
        ArkLogger.e(this, "save this=" + this);
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);

            int version = 1;
            os.writeInt(version);
            os.writeInt(pageId);
            os.writeLong(tabInfoId);
            os.writeBoolean(isIncognito);
            os.writeBoolean(fromMerge);
            os.writeInt(themeColor);
            os.writeInt(originalIndex);
            os.writeUTF(url);
            os.writeUTF(title);
            os.close();

            byte[] bytes = stream.toByteArray();

            ThreadPool.executeIO(new Runnable() {
                @Override
                public void run() {
                    File pagesDir = ArkTabDao.getPagesDir(tabInfoId);
                    AtomicFile file = new AtomicFile(new File(pagesDir, String.valueOf(pageId)));
                    FileOutputStream fos = null;
                    try {
                        fos = file.startWrite();
                        fos.write(bytes, 0, bytes.length);
                        file.finishWrite(fos);
                    } catch (IOException e) {
                        if (fos != null) file.failWrite(fos);
                        ArkLogger.e(this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
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
//                os.writeLong(pageInfo.tabInfoId);
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
