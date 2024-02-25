// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;
import android.content.Intent;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.media.ui.ChromeMediaNotificationManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.content_public.browser.WebContents;

/** Provides Chrome-specific behavior for Media Router. */
@JNINamespace("media_router")
public class ChromeMediaRouterClient extends MediaRouterClient {
    private ChromeMediaRouterClient() {}

    @Override
    public Context getContextForRemoting() {
        return ContextUtils.getApplicationContext();
    }

    @Override
    public int getTabId(WebContents webContents) {
        Tab tab = TabUtils.fromWebContents(webContents);
        return tab == null ? -1 : tab.getId();
    }

    @Override
    public Intent createBringTabToFrontIntent(int tabId) {
        return IntentHandler.createTrustedBringTabToFrontIntent(
                tabId, IntentHandler.BringToFrontSource.NOTIFICATION);
    }

    @Override
    public void showNotification(MediaNotificationInfo notificationInfo) {
        ChromeMediaNotificationManager.show(notificationInfo);
    }

    @Override
    public int getPresentationNotificationId() {
        return R.id.presentation_notification;
    }

    @Override
    public int getRemotingNotificationId() {
        return R.id.remote_playback_notification;
    }

    @Override
    public FragmentManager getSupportFragmentManager(WebContents initiator) {
        FragmentActivity currentActivity =
                (FragmentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        return currentActivity == null ? null : currentActivity.getSupportFragmentManager();
    }

    @Override
    public void addDeferredTask(Runnable deferredTask) {
        DeferredStartupHandler.getInstance().addDeferredTask(deferredTask);
        DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
    }

    @CalledByNative
    public static void initialize() {
        if (MediaRouterClient.getInstance() != null) return;

        MediaRouterClient.setInstance(new ChromeMediaRouterClient());
    }
}
