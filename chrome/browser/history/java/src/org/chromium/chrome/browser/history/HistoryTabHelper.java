// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.content_public.browser.WebContents;

/**
 * History helper class. Configures native WebContents objects at initialization or when switched to
 * a new one.
 */
@NullMarked
public class HistoryTabHelper extends TabWebContentsUserData {

    private static final Class<HistoryTabHelper> USER_DATA_KEY = HistoryTabHelper.class;
    private @Nullable String mAppId;

    public static HistoryTabHelper from(Tab tab) {
        HistoryTabHelper handler = get(tab);
        if (handler == null) {
            handler = attachToTab(USER_DATA_KEY, new HistoryTabHelper(tab));
        }
        return handler;
    }

    public static @Nullable HistoryTabHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private HistoryTabHelper(Tab tab) {
        super(tab);
    }

    /**
     * @param appId App ID
     */
    public void setAppId(String appId, WebContents webContents) {
        mAppId = appId;
        setAppId(webContents);
    }

    public void setAppIdForViewIntent(String appId, WebContents webContents) {
        // For view intent, appId is stored in the native class and nulled out
        // after the first commit.
        assert webContents != null : "WebContents should be non-null";
        HistoryTabHelperJni.get().setAppIdForViewIntentNative(appId, webContents);
    }

    private void setAppId(WebContents webContents) {
        assert webContents != null : "WebContents should be non-null";
        HistoryTabHelperJni.get().setAppIdNative(mAppId, webContents);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        // Avoid a redundant null JNI call during from(tab) before setAppId() or
        // setAppIdForViewIntent() runs; native HistoryTabHelper defaults app_id_ to nullopt.
        if (mAppId != null) {
            setAppId(webContents);
        }
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    @Nullable
    public static String getAppIdForTesting(WebContents webContents) {
        assert webContents != null : "WebContents should be non-null";
        return HistoryTabHelperJni.get().getAppIdForTestingNative(webContents);
    }

    @NativeMethods
    interface Natives {
        void setAppIdNative(
                @JniType("std::optional<std::string>") @Nullable String appId,
                WebContents webContents);

        void setAppIdForViewIntentNative(
                @JniType("std::optional<std::string>") @Nullable String appId,
                WebContents webContents);

        @JniType("std::optional<std::string>")
        @Nullable String getAppIdForTestingNative(WebContents webContents);
    }
}
