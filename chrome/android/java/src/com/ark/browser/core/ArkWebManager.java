//package com.ark.browser.core;
//
//import android.util.SparseArray;
//
//import com.ark.browser.tab.PageInfo;
//
//public class ArkWebManager {
//
//    private final SparseArray<ArkWebContents> tabCache = new SparseArray<>();
//
//    private static final class Holder {
//        private static final ArkWebManager MANAGER = new ArkWebManager();
//    }
//
//    public static ArkWebManager getInstance() {
//        return Holder.MANAGER;
//    }
//
//    private ArkWebManager() {
//
//    }
//
//    public ArkWebContents remove(int id) {
//        ArkWebContents arkWeb = tabCache.get(id, null);
//        if (arkWeb != null) {
//            tabCache.remove(id);
//            if (arkWeb.isDestroyed()) {
//                arkWeb = null;
//            }
//        }
//        return arkWeb;
//    }
//
//    public void put(int id, ArkWebContents arkWeb) {
//        tabCache.put(id, arkWeb);
//    }
//
//    public ArkWebContents obtain(PageInfo pageInfo) {
//        ArkWebContents webContents = tabCache.get(pageInfo.getPageId(), null);
//        if (webContents == null || webContents.isDestroyed()) {
//            webContents = new ArkWebContents.Builder(pageInfo).build();
//            tabCache.put(pageInfo.getPageId(), webContents);
//        }
//        return webContents;
//    }
//
//}
