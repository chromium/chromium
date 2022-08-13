package com.ark.browser.tab.core;

public interface IPageInfo {

    public int getPageId();

    public void setPageId(int pageId);

    public int getParentId();

    public void setParentId(int parentId);

    public String getTabInfoId();

    public void setTabInfoId(String tabInfoId);

    public int getOriginalIndex();

    public void setOriginalIndex(int originalIndex);

    public Boolean isIncognito();

    public void setIncognito(Boolean incognito);

    public Boolean getFromMerge();

    public void setFromMerge(Boolean fromMerge);

    public int getThemeColor();

    public void setThemeColor(int themeColor);

    public String getUrl();

    public void setUrl(String url);

    public String getTitle();

    public void setTitle(String title);

}
