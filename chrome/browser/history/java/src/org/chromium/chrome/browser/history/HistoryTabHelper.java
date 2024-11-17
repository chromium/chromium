// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.content_public.browser.WebContents;

/**
 * History helper class. Configures native WebContents objects at initialization or when switched to
 * a new one.
 */
public class HistoryTabHelper extends TabWebContentsUserData {

    private static final Class<HistoryTabHelper> USER_DATA_KEY = HistoryTabHelper.class;
    private String mAppId;

    public static HistoryTabHelper from(Tab tab) {
        HistoryTabHelper handler = get(tab);
        if (handler == null) {
            handler = tab.getUserDataHost().setUserData(USER_DATA_KEY, new HistoryTabHelper(tab));
        }
        return handler;
    }

    private static @Nullable HistoryTabHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private HistoryTabHelper(Tab tab) {
        super(tab);
    }

    /**
     * @param appId App ID
     */
    public void setAppId(String appId, @NonNull WebContents webContents) {
        mAppId = appId;
        setAppId(webContents);
    }

    private void setAppId(WebContents webContents) {
        assert webContents != null : "WebContents should be non-null";
        HistoryTabHelperJni.get().setAppIdNative(mAppId, webContents);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        setAppId(webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    @NativeMethods
    interface Natives {
        void setAppIdNative(String appId, WebContents webContents);
    }
}
