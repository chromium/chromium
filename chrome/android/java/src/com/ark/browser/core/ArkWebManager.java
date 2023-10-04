package com.ark.browser.core;

import android.util.SparseArray;

import com.ark.browser.utils.PrefsHelper;
import com.ark.browser.utils.ThreadPool;

import org.chromium.content_public.browser.WebContents;

public class ArkWebManager {

    public static final int POLICY_0 = 0;
    public static final int POLICY_1 = 1;
    public static final int POLICY_NORMAL = 2;
    public static final int POLICY_3 = 3;
    public static final int POLICY_4 = 4;
    public static final int POLICY_5 = 5;

    private static int sMaxWebCount = 8;
    private static final SparseArray<ArkWebContents> PAGE_CACHE = new SparseArray<>();

    static {
        init(getKeepAlivePolicy());
    }

    private static void init(int policy) {
        if (policy < POLICY_5) {
            sMaxWebCount = (int) Math.pow(2, policy + 1);
        } else {
            sMaxWebCount = Integer.MAX_VALUE;
        }
    }

    public static void setKeepAlivePolicy(int policy) {
        PrefsHelper.with().applyInt("web_keep_alive_policy", policy);
        init(policy);
    }

    public static int getKeepAlivePolicy() {
        return PrefsHelper.with().getInt("web_keep_alive_policy", POLICY_NORMAL);
    }

    public static ArkWebContents remove(int id) {
        ArkWebContents arkWeb = get(id);
        if (arkWeb != null) {
            PAGE_CACHE.remove(id);
            if (arkWeb.isDestroyed()) {
                arkWeb = null;
            } else {
                arkWeb.destroy();
            }
        }
        return arkWeb;
    }

    public static void destroy() {
        for (int i = 0; i < PAGE_CACHE.size(); i++) {
            ArkWebContents web = PAGE_CACHE.valueAt(i);
            if (web != null && !web.isDestroyed()) {
                web.destroy();
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
        remove(id);
        PAGE_CACHE.put(id, arkWeb);
        if (PAGE_CACHE.size() > sMaxWebCount) {
            ThreadPool.postOnUIThread(new Runnable() {
                @Override
                public void run() {
                    if (PAGE_CACHE.size() > sMaxWebCount) {
                        int key = PAGE_CACHE.keyAt(0);
                        long min = PAGE_CACHE.valueAt(0).getLastVisitTime();
                        for (int i = 1; i < PAGE_CACHE.size(); i++) {
                            ArkWebContents web = PAGE_CACHE.valueAt(i);
                            if (web.getLastVisitTime() < min) {
                                min = web.getLastVisitTime();
                                key = PAGE_CACHE.keyAt(i);
                            }
                        }
                        remove(key);
                    }
                }
            });
        }
    }

}
