package com.ark.browser.ui.dialog;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.core.UserAgentManager;
import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.tab.Tab;

import java.util.Arrays;

public class UserAgentSelector {

    public static void show(ArkBrowserActivity activity) {
        Tab tab = activity.getActivityTab();

        String title;
        String host;
        int index;
        String[] items;
        if (tab == null) {
            title = "选择浏览器标识";
            host = "default";
            index = UserAgentManager.getDefaultUserAgentIndex();
            items = UserAgentManager.getUserAgentNames();
        } else {
            host = tab.getUrl().getHost();
            title = "选择浏览器标识:" + host;
            index = UserAgentManager.getUserAgentIndexByUrl(host) + 1;
            String[] names = UserAgentManager.getUserAgentNames();
            items = new String[names.length + 1];
            items[0] = "默认UA";
            System.arraycopy(names, 0, items, 1, names.length);
        }
        ArkLogger.e(ArkBrowserActivity.class, "showUserAgentSelector index=" + index + " items=" + Arrays.toString(items));
        AlertDialog selector = new AlertDialog.Builder(activity)
                .setTitle(title)
                .setSingleChoiceItems(items, index, (dialog, which) -> {
                    if (tab == null) {
                        UserAgentManager.setDefaultUserAgentIndex(which);
                    } else {
                        which -= 1;
                        UserAgentManager.setUserAgentByUrl(host, which);
                        if (tab.getWebContents() != null) {
                            UserAgentManager.UserAgent ua = UserAgentManager.getUserAgent(which);
                            ContentUtils.setUserAgentOverride(tab.getWebContents(), ua);
                        }
                    }

                    dialog.dismiss();
                })
                .create();
        selector.show();
    }

}
