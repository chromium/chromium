// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentRemoval;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Provides access to native implementations of OfflineIndicatorApi. */
@JNINamespace("feed")
public class FeedOfflineBridge implements FeedOfflineIndicator, KnownContent.Listener {
    private long mNativeBridge;
    private KnownContent mKnownContentApi;

    /**
     * Hold onto listeners in Java. It is difficult to offload this completely to native, because we
     * need to remove with object reference equality in removeOfflineStatusListener().
     */
    private Set<OfflineStatusListener> mListeners = new HashSet<>();

    /**
     * Creates a FeedOfflineBridge for accessing native offlining logic.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     * @param knownContent Interface to access information about the Feed's articles.
     */
    public FeedOfflineBridge(Profile profile, KnownContent knownContent) {
        mNativeBridge = FeedOfflineBridgeJni.get().init(FeedOfflineBridge.this, profile);
        mKnownContentApi = knownContent;
        mKnownContentApi.addListener(this);
    }

    @Override
    public void destroy() {
        assert mNativeBridge != 0;
        FeedOfflineBridgeJni.get().destroy(mNativeBridge, FeedOfflineBridge.this);
        mNativeBridge = 0;
        mKnownContentApi.removeListener(this);
    }

    @Override
    public Long getOfflineIdIfPageIsOfflined(String url) {
        if (mNativeBridge == 0) {
            return 0L;
        } else {
            return (Long) FeedOfflineBridgeJni.get().getOfflineId(
                    mNativeBridge, FeedOfflineBridge.this, url);
        }
    }

    @Override
    public void getOfflineStatus(
            List<String> urlsToRetrieve, Consumer<List<String>> urlListConsumer) {
        if (mNativeBridge == 0) {
            urlListConsumer.accept(Collections.emptyList());
        } else {
            String[] urlsArray = urlsToRetrieve.toArray(new String[urlsToRetrieve.size()]);
            FeedOfflineBridgeJni.get().getOfflineStatus(mNativeBridge, FeedOfflineBridge.this,
                    urlsArray,
                    (String[] urlsAsArray) -> urlListConsumer.accept(Arrays.asList(urlsAsArray)));
        }
    }

    @Override
    public void addOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        if (mNativeBridge != 0) {
            mListeners.add(offlineStatusListener);
        }
    }

    @Override
    public void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        if (mNativeBridge != 0) {
            mListeners.remove(offlineStatusListener);
            if (mListeners.isEmpty()) {
                FeedOfflineBridgeJni.get().onNoListeners(mNativeBridge, FeedOfflineBridge.this);
            }
        }
    }

    @Override
    public void onContentRemoved(List<ContentRemoval> contentRemoved) {
        if (mNativeBridge != 0) {
            List<String> userDrivenRemovals = takeUserDriveRemovalsOnly(contentRemoved);
            if (!userDrivenRemovals.isEmpty()) {
                FeedOfflineBridgeJni.get().onContentRemoved(mNativeBridge, FeedOfflineBridge.this,
                        userDrivenRemovals.toArray(new String[userDrivenRemovals.size()]));
            }
        }
    }

    @Override
    public void onNewContentReceived(boolean isNewRefresh, long contentCreationDateTimeMs) {
        if (mNativeBridge != 0) {
            FeedOfflineBridgeJni.get().onNewContentReceived(mNativeBridge, FeedOfflineBridge.this);
        }
    }

    /**
     * Filters out any {@link ContentRemoval} that was not user driven, such as old articles being
     * garbage collected.
     *
     * @param contentRemoved The articles being removed, may or may not be user driven.
     * @return All and only the user driven removals.
     */
    @VisibleForTesting
    static List<String> takeUserDriveRemovalsOnly(List<ContentRemoval> contentRemoved) {
        List<String> urlsRemovedByUser = new ArrayList<>();
        for (ContentRemoval removal : contentRemoved) {
            if (removal.isRequestedByUser()) {
                urlsRemovedByUser.add(removal.getUrl());
            }
        }
        return urlsRemovedByUser;
    }

    @CalledByNative
    private static Long createLong(long id) {
        return Long.valueOf(id);
    }

    @CalledByNative
    private void getKnownContent() {
        mKnownContentApi.getKnownContent((List<ContentMetadata> metadataList) -> {
            if (mNativeBridge == 0) return;

            for (ContentMetadata metadata : metadataList) {
                long time_published_ms = TimeUnit.SECONDS.toMillis(metadata.getTimePublished());
                FeedOfflineBridgeJni.get().appendContentMetadata(mNativeBridge,
                        FeedOfflineBridge.this, metadata.getUrl(), metadata.getTitle(),
                        time_published_ms, metadata.getImageUrl(), metadata.getPublisher(),
                        metadata.getFaviconUrl(), metadata.getSnippet());
            }
            FeedOfflineBridgeJni.get().onGetKnownContentDone(mNativeBridge, FeedOfflineBridge.this);
        });
    }

    @CalledByNative
    private void notifyStatusChange(String url, boolean availableOffline) {
        for (OfflineStatusListener listener : mListeners) {
            listener.updateOfflineStatus(url, availableOffline);
        }
    }

    @NativeMethods
    interface Natives {
        long init(FeedOfflineBridge caller, Profile profile);
        void destroy(long nativeFeedOfflineBridge, FeedOfflineBridge caller);
        Object getOfflineId(long nativeFeedOfflineBridge, FeedOfflineBridge caller, String url);
        void getOfflineStatus(long nativeFeedOfflineBridge, FeedOfflineBridge caller,
                String[] urlsToRetrieve, Callback<String[]> urlListConsumer);
        void onContentRemoved(
                long nativeFeedOfflineBridge, FeedOfflineBridge caller, String[] urlsRemoved);
        void onNewContentReceived(long nativeFeedOfflineBridge, FeedOfflineBridge caller);
        void onNoListeners(long nativeFeedOfflineBridge, FeedOfflineBridge caller);
        void appendContentMetadata(long nativeFeedOfflineBridge, FeedOfflineBridge caller,
                String url, String title, long timePublishedMs, String imageUrl, String publisher,
                String faviconUrl, String snippet);
        void onGetKnownContentDone(long nativeFeedOfflineBridge, FeedOfflineBridge caller);
    }
}
