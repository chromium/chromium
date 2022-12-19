package com.ark.browser.tab;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.core.util.AtomicFile;

import com.ark.browser.core.utils.ArkIdManager;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

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

//    @Column
    public int tabId;
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

    public static PageInfo from(int tabId, int index, boolean isIncognito) {
        PageInfo info = new PageInfo();
        info.pageId = ArkIdManager.generatePageId();
        info.originalIndex = index;
        info.tabId = tabId;
        info.isIncognito = isIncognito;
        info.fromMerge = false;
        info.save();
        return info;
    }

    public static PageInfo from(PageInfo pageInfo) {
        PageInfo info = new PageInfo();
        info.pageId = pageInfo.pageId;
        info.originalIndex = pageInfo.originalIndex;
        info.tabId = pageInfo.tabId;
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
        info.tabId = is.readInt();
        info.isIncognito = is.readBoolean();
        info.fromMerge = is.readBoolean();
        info.themeColor = is.readInt();
        info.originalIndex = is.readInt();
        info.url = is.readUTF();
        info.title = is.readUTF();
        return info;
    }

    public int getId() {
        return pageId;
    }

    public void setId(int pageId) {
        this.pageId = pageId;
    }

    public int getTabId() {
        return tabId;
    }

    public void setTabId(int tabId) {
        this.tabId = tabId;
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
                ", tabInfoId='" + tabId + '\'' +
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
            os.writeInt(tabId);
            os.writeBoolean(isIncognito);
            os.writeBoolean(fromMerge);
            os.writeInt(themeColor);
            os.writeInt(originalIndex);
            os.writeUTF(url == null ? "" : url);
            os.writeUTF(title == null ? "" : title);
            os.close();

            byte[] bytes = stream.toByteArray();

            ThreadPool.executeIO(new Runnable() {
                @Override
                public void run() {
                    File pagesDir = ArkTabDao.getPagesDir(tabId);
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
