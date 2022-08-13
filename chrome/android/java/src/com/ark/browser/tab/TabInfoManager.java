package com.ark.browser.tab;

import java.util.ArrayList;
import java.util.List;

//@Database(name = TabInfoManager.NAME, version = TabInfoManager.VERSION)
public class TabInfoManager {

    private static final String TAG = "TabInfoManager";

    static final String NAME = "tab_info_list";
    public static final int VERSION = 1;


    public static List<TabInfo> getAllTabs() {

        return new ArrayList<>();

//        return SQLite.select()
//                .from(TabInfo.class)
//                .orderBy(TabInfo_Table.position, true)
//                .queryList();
    }

}
