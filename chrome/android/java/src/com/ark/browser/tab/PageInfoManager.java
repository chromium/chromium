package com.ark.browser.tab;

import java.util.ArrayList;
import java.util.List;

//@Database(name = PageInfoManager.NAME, version = PageInfoManager.VERSION)
public class PageInfoManager {

    private static final String TAG = "PageInfoManager";

    static final String NAME = "page_info_list";
    public static final int VERSION = 1;

    public static List<PageInfo> getAllPages() {

        return new ArrayList<>();

//        return SQLite.select()
//                .from(PageInfo.class)
//                .orderBy(PageInfo_Table.originalIndex, true)
//                .queryList();
    }

    public static List<PageInfo> getAllPages(String managerId) {
        return new ArrayList<>();

//        return SQLite.select()
//                .from(PageInfo.class)
//                .where(PageInfo_Table.tabInfoId.eq(managerId))
//                .orderBy(PageInfo_Table.originalIndex, true)
//                .queryList();
    }

    public static List<PageInfo> getAllBreakPages() {
        return new ArrayList<>();
//        return SQLite.select()
//                .from(PageInfo.class)
//                .where(PageInfo_Table.tabInfoId.eq("0"))
//                .orderBy(PageInfo_Table.originalIndex, true)
//                .queryList();
    }


}
