package com.ark.browser.core;

import android.util.SparseArray;

import org.chromium.content_public.browser.WebContents;

public class ArkWebManager {

    private static final SparseArray<ArkWebContents> PAGE_CACHE = new SparseArray<>();

    public static ArkWebContents remove(int id) {
        ArkWebContents arkWeb = get(id);
        if (arkWeb != null) {
            PAGE_CACHE.remove(id);
            if (arkWeb.isDestroyed()) {
                arkWeb = null;
            } else {
                arkWeb.getWebContents().destroy();
            }
        }
        return arkWeb;
    }

    public static void destroy() {
        for (int i = 0; i < PAGE_CACHE.size(); i++) {
            ArkWebContents web = PAGE_CACHE.valueAt(i);
            if (web != null && !web.isDestroyed()) {
                web.getWebContents().destroy();
            }
        }
        PAGE_CACHE.clear();
    }

    public static ArkWebContents get(int id) {
        return PAGE_CACHE.get(id, null);
    }

    public static WebContents getWebContents(int id) {
        ArkWebContents web = get(id);
        if (web == null) {
            return null;
        }
        return web.getWebContents();
    }

    public static void put(int id, ArkWebContents arkWeb) {
        PAGE_CACHE.put(id, arkWeb);
    }

}
