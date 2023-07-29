package com.ark.browser.tab.core;

import java.util.List;

public interface IPageGroup {

    List<IPage> getPages();

    IPage getPageAt(int index);

    int getPageSize();

    int indexOfPage(int pageId);

}
