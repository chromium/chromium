package com.ark.browser.tab;

import androidx.annotation.Keep;

@Keep
//@Table(database = PageInfoManager.class)
public class PageInfo extends DbModel {

//    @PrimaryKey()
    public int pageId;

//    @Column(defaultValue = "-1")
    public int parentId;

//    @Column
    public String tabInfoId;
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

    public static PageInfo from(int pageId, int parentId, String tabInfoId, int index, boolean isIncognito) {
        PageInfo info = new PageInfo();
        info.pageId = pageId;
        info.parentId = parentId;
        info.originalIndex = index;
        info.tabInfoId = tabInfoId;
        info.isIncognito = isIncognito;
        info.fromMerge = false;
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

    public String getTabInfoId() {
        return tabInfoId;
    }

    public void setTabInfoId(String tabInfoId) {
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
        this.url = url;
    }

    public String getTitle() {
        return title == null ? getUrl() : title;
    }

    public void setTitle(String title) {
        this.title = title;
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
}
