// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.feed.FeedListContentManager.FeedContent;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;

import java.util.List;

/** Interface used for interacting with the Stream library in order to render a stream of cards. */
public interface Stream {
    /** The mediator of multiple Streams. */
    public interface StreamsMediator {
        /**
         * Allows the switching to another Stream.
         * @param streamKind The {@link StreamKind} of the stream to switch to.
         */
        default void switchToStreamKind(@StreamKind int streamKind) {}

        /** Request the immediate refresh of the contents of the active stream. */
        default void refreshStream() {}

        /** Disable the follow button, used in case of an error scenario. */
        default void disableFollowButton() {}
    }

    /** Called when the Stream is no longer needed. */
    default void destroy() {}

    /** Returns the section type for this stream. */
    @StreamKind
    int getStreamKind();

    /**
     * @param scrollState Previous saved scroll state to restore to.
     */
    void restoreSavedInstanceState(FeedScrollState scrollState);

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
     * <p>Note: this will assume {@link RequestReason.MANUAL_REFRESH}.
     */
    void triggerRefresh(Callback<Boolean> callback);

    /** Whether activity logging is enabled for this feed. */
    default boolean isActivityLoggingEnabled() {
        return false;
    }

    /** Whether the stream has unread content */
    default ObservableSupplier<Boolean> hasUnreadContent() {
        ObservableSupplierImpl<Boolean> result = new ObservableSupplierImpl<>();
        result.set(false);
        return result;
    }

    /** Returns the last content fetch time. */
    default long getLastFetchTimeMs() {
        return 0;
    }

    /**
     * Binds the feed to a particular view, manager, and scope.
     * When bound, the feed actively updates views and content. Assumes that whatever
     * views currently shown by manager are headers.
     *  @param view The {@link RecyclerView} to which the feed is bound.
     * @param manager The {@link FeedListContentManager} to which we should make updates to.
     * @param savedInstanceState A previously saved instance state to restore to after loading
     *         content.
     * @param surfaceScope The {@link FeedSurfaceScope} that is hosting the renderer.
     * @param renderer The {@link HybridListRenderer} that is rendering the feed.
     * @param reliabilityLogger Logger for feed reliability.
     * @param headerCount The number of headers in the RecyclerView that the feed shouldn't touch.
     */
    void bind(
            RecyclerView view,
            FeedListContentManager manager,
            FeedScrollState savedInstanceState,
            FeedSurfaceScope surfaceScope,
            HybridListRenderer renderer,
            @Nullable FeedReliabilityLogger reliabilityLogger,
            int headerCount);

    /**
     * Unbinds the feed. Stops this feed from updating the RecyclerView.
     *
     * @param shouldPlaceSpacer Whether this feed should place a spacer at the end to
     *     prevent abrupt scroll jumps.
     * @param switchingStream Whether another feed is going to be bound right after this.
     */
    void unbind(boolean shouldPlaceSpacer, boolean switchingStream);

    /** Whether this stream supports alternate sort options. */
    default boolean supportsOptions() {
        return false;
    }

    /**
     * Returns a value that uniquely identifies the state of the Stream's content. If this value
     * changes, then scroll state won't be restored.
     */
    String getContentState();

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
