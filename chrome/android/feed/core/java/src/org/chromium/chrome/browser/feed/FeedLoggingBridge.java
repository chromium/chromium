// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.logging.ActionType;
import com.google.android.libraries.feed.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.host.logging.ContentLoggingData;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed,
 * and provides access to native implementation of feed logging.
 */
@JNINamespace("feed")
public class FeedLoggingBridge implements BasicLoggingApi {
    private long mNativeFeedLoggingBridge;

    /**
     * Creates a {@link FeedLoggingBridge} for accessing native feed logging
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedLoggingBridge(Profile profile) {
        mNativeFeedLoggingBridge = nativeInit(profile);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedLoggingBridge != 0;
        nativeDestroy(mNativeFeedLoggingBridge);
        mNativeFeedLoggingBridge = 0;
    }

    @Override
    public void onContentViewed(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentViewed(mNativeFeedLoggingBridge, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()), data.getScore());
    }

    @Override
    public void onContentDismissed(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentDismissed(
                mNativeFeedLoggingBridge, data.getPositionInStream(), data.getRepresentationUri());
    }

    @Override
    public void onContentClicked(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentClicked(mNativeFeedLoggingBridge, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore());
    }

    @Override
    public void onClientAction(ContentLoggingData data, @ActionType int actionType) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnClientAction(
                mNativeFeedLoggingBridge, feedActionToWindowOpenDisposition(actionType));
    }

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentContextMenuOpened(mNativeFeedLoggingBridge, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore());
    }

    @Override
    public void onMoreButtonViewed(int position) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnMoreButtonViewed(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onMoreButtonClicked(int position) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnMoreButtonClicked(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithContent(mNativeFeedLoggingBridge, timeToPopulateMs, contentCount);
    }

    @Override
    public void onOpenedWithNoImmediateContent() {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithNoImmediateContent(mNativeFeedLoggingBridge);
    }

    @Override
    public void onOpenedWithNoContent() {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithNoContent(mNativeFeedLoggingBridge);
    }

    /**
     * Reports how long a user spends on the page.
     *
     * @param visitTimeMs Time spent reading the page.
     */
    public void onContentTargetVisited(long visitTimeMs) {
        // We cannot assume that the |mNativeFeedLoggingBridge| is always available like other
        // methods. This method is called by objects not controlled by Feed lifetimes, and destroy()
        // may have already been called if Feed is disabled by policy.
        if (mNativeFeedLoggingBridge != 0) {
            nativeOnContentTargetVisited(mNativeFeedLoggingBridge, visitTimeMs);
        }
    }

    /**
     * Reports how long a user spends on the offline page.
     *
     * @param visitTimeMs Time spent reading the page.
     */
    public void onOfflinePageVisited(long visitTimeMs) {
        // We cannot assume that the |mNativeFeedLoggingBridge| is always available like other
        // methods. This method is called by objects not controlled by Feed lifetimes, and destroy()
        // may have already been called if Feed is disabled by policy.
        if (mNativeFeedLoggingBridge != 0) {
            nativeOnOfflinePageVisited(mNativeFeedLoggingBridge, visitTimeMs);
        }
    }

    private int feedActionToWindowOpenDisposition(@ActionType int actionType) {
        switch (actionType) {
            case ActionType.OPEN_URL:
                return WindowOpenDisposition.CURRENT_TAB;
            case ActionType.OPEN_URL_INCOGNITO:
                return WindowOpenDisposition.IGNORE_ACTION;
            case ActionType.OPEN_URL_NEW_TAB:
                return WindowOpenDisposition.NEW_BACKGROUND_TAB;
            case ActionType.OPEN_URL_NEW_WINDOW:
                return WindowOpenDisposition.NEW_WINDOW;
            case ActionType.DOWNLOAD:
                return WindowOpenDisposition.SAVE_TO_DISK;
            case ActionType.LEARN_MORE:
            case ActionType.UNKNOWN:
            default:
                return WindowOpenDisposition.UNKNOWN;
        }
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedLoggingBridge);
    private native void nativeOnContentViewed(long nativeFeedLoggingBridge, int position,
            long publishedTimeMs, long timeContentBecameAvailableMs, float score);
    private native void nativeOnContentDismissed(
            long nativeFeedLoggingBridge, int position, String uri);
    private native void nativeOnContentClicked(
            long nativeFeedLoggingBridge, int position, long publishedTimeMs, float score);
    private native void nativeOnClientAction(
            long nativeFeedLoggingBridge, int windowOpenDisposition);
    private native void nativeOnContentContextMenuOpened(
            long nativeFeedLoggingBridge, int position, long publishedTimeMs, float score);
    private native void nativeOnMoreButtonViewed(long nativeFeedLoggingBridge, int position);
    private native void nativeOnMoreButtonClicked(long nativeFeedLoggingBridge, int position);
    private native void nativeOnOpenedWithContent(
            long nativeFeedLoggingBridge, int timeToPopulateMs, int contentCount);
    private native void nativeOnOpenedWithNoImmediateContent(long nativeFeedLoggingBridge);
    private native void nativeOnOpenedWithNoContent(long nativeFeedLoggingBridge);
    private native void nativeOnContentTargetVisited(
            long nativeFeedLoggingBridge, long visitTimeMs);
    private native void nativeOnOfflinePageVisited(long nativeFeedLoggingBridge, long visitTimeMs);
}
