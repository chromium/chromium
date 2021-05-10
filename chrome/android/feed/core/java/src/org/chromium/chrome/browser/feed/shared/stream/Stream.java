// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared.stream;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.feed.FeedSurfaceMediator;
import org.chromium.chrome.browser.feed.NtpListContentManager;
import org.chromium.chrome.browser.feed.NtpListContentManager.FeedContent;
import org.chromium.chrome.browser.ntp.snippets.SectionType;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.SurfaceScope;

import java.util.List;

/** Interface used for interacting with the Stream library in order to render a stream of cards. */
public interface Stream {
    /** Called when the Stream is no longer needed. */
    default void destroy() {}

    /** Returns the section type for this stream. */
    @SectionType
    int getSectionType();

    /**
     * @param scrollState Previous saved scroll state to restore to.
     */
    void restoreSavedInstanceState(FeedSurfaceMediator.ScrollState scrollState);

    /**
     * Record that visibility of the feed was toggled through the header menu. Note that
     * bind() should be called separately when this happens.
     */
    default void toggledArticlesListVisible(boolean visible) {}

    /**
     * Notifies that the header count has changed. Headers are views added to the Recyclerview
     * that the stream should not delete.
     *
     * @param newHeaderCount The new number of headers.
     */
    void notifyNewHeaderCount(int newHeaderCount);

    /**
     * @param listener A {@link ContentChangedListener} which activates when the content changes
     * while the stream is bound.
     */
    void addOnContentChangedListener(ContentChangedListener listener);

    /**
     * @param listener A previously added {@link ContentChangedListener} to be removed. Will no
     *         longer trigger after removal.
     */
    void removeOnContentChangedListener(ContentChangedListener listener);

    /**
     * Allow the container to trigger a refresh of the stream.
     *
     * <p>Note: this will assume {@link RequestReason.HOST_REQUESTED}.
     */
    void triggerRefresh();

    /**
     * @return Whether the placeholder is shown.
     */
    boolean isPlaceholderShown();

    /**
     * Called when the placeholder is shown and the first batch of articles are about to show.
     */
    void hidePlaceholder();

    /** Record that user tapped ManageInterests. */
    default void recordActionManageInterests() {}

    /** Record that user tapped Manage Activity. */
    default void recordActionManageActivity() {}

    /** Record that user tapped Manage Reactions. */
    default void recordActionManageReactions() {}

    /** Record that user tapped Learn More. */
    default void recordActionLearnMore() {}

    /** @returns Whether we should be logging user activity. */
    default boolean isActivityLoggingEnabled() {
        return false;
    }

    /** @returns Experiment IDs applicable to this feed. */
    default int[] getExperimentIds() {
        return new int[0];
    }

    /** @returns The session ID to use if user is signed out. */
    default String getSignedOutSessionId() {
        return "";
    }

    /** Whether the stream has unread content */
    default ObservableSupplier<Boolean> hasUnreadContent() {
        ObservableSupplierImpl<Boolean> result = new ObservableSupplierImpl<>();
        result.set(false);
        return result;
    }

    /**
     * Binds the feed to a particular view, manager, and scope.
     * When bound, the feed actively updates views and content. Assumes that whatever
     * views currently shown by manager are headers.
     *
     * @param view The {@link RecyclerView} to which the feed is bound.
     * @param manager The {@link NtpListContentManager} to which we should make updates to.
     * @param savedInstanceState A previously saved instance state to restore to after loading
     *         content.
     * @param surfaceScope The {@link SurfaceScope} that is hosting the renderer.
     * @param renderer The {@link HybridListRenderer} that is rendering the feed.
     */
    void bind(RecyclerView view, NtpListContentManager manager,
            FeedSurfaceMediator.ScrollState savedInstanceState, SurfaceScope surfaceScope,
            HybridListRenderer renderer);

    /**
     * Unbinds the feed. Removes all views this feed has added to the previously bound
     * content manager.
     */
    void unbind();

    /**
     * Interface users can implement to know when content in the Stream has changed content on
     * screen.
     */
    interface ContentChangedListener {
        /**
         * Called by Stream when content being shown has changed. This could be new cards being
         * created, the content of a card changing, etc...
         * @param feedContents the list of feed contents after the change. Null if the contents are
         *         not available.
         */
        void onContentChanged(@Nullable List<FeedContent> feedContents);
    }
}
