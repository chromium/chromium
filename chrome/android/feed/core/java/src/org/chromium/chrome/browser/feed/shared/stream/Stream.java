// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared.stream;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ntp.ScrollListener;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Interface used for interacting with the Stream library in order to render a stream of cards. */
public interface Stream {
    /** Constant used to notify host that a view's position on screen is not known. */
    int POSITION_NOT_KNOWN = Integer.MIN_VALUE;

    @IntDef({FeedFirstCardDensity.UNKNOWN, FeedFirstCardDensity.NOT_DENSE,
            FeedFirstCardDensity.DENSE})
    @Retention(RetentionPolicy.SOURCE)
    @interface FeedFirstCardDensity {
        int UNKNOWN = 0;
        int NOT_DENSE = 1;
        int DENSE = 2;
    }

    /**
     * Called when the Stream is being created. {@code savedInstanceState} should be a previous
     * string returned from {@link #getSavedInstanceStateString()}.
     *
     * @param savedInstanceState state to restore to.
     * @throws IllegalStateException if method is called multiple times.
     */
    void onCreate(@Nullable String savedInstanceState);

    /**
     * Called when the Stream is visible on the screen, may be partially obscured or about to be
     * displayed. This maps similarly to {@link android.app.Activity#onStart()}.
     *
     * <p>This will cause the Stream to start pre-warming feed services.
     */
    void onShow();

    /**
     * Called when the Stream is no longer visible on screen. This should act similarly to {@link
     * android.app.Activity#onStop()}.
     */
    void onHide();

    /**
     * Called when the Stream is destroyed. This should act similarly to {@link
     * android.app.Activity#onDestroy()}.
     */
    void onDestroy();

    /**
     * The returned string should be passed to {@link #onCreate(String)} when the activity is
     * recreated and the stream needs to be recreated.
     */
    String getSavedInstanceStateString();

    /**
     * Return the root view which holds all card stream views. The Feed library builds Views when
     * this method is called (caches as needed). This must be called after {@link
     * #onCreate()}. Multiple calls to this method will return the same View.
     *
     * @throws IllegalStateException when called before {@link #onCreate()}.
     */
    View getView();

    /**
     * Set headers on the stream. Headers will be added to the top of the stream in the order they
     * are in the list. The headers are not sticky and will scroll with content. Headers can be
     * cleared by passing in an empty list.
     */
    void setHeaderViews(List<Header> headers);

    /**
     * Sets whether or not the Stream should show Feed content. Header views will still be shown if
     * set.
     */
    void setStreamContentVisibility(boolean visible);

    /**
     * Visibility of the feed was toggled through the header menu. Note that
     * setStreamContentVisibility() is also called when this happens.
     */
    default void toggledArticlesListVisible(boolean visible) {}

    /**
     * Notifies the Stream to purge unnecessary memory. This just purges recycling pools for now.
     * Can expand out as needed.
     */
    void trim();

    /**
     * Called by the host to scroll the Stream by a certain amount. If the Stream is unable to
     * scroll the desired amount, it will scroll as much as possible.
     *
     * @param dx amount in pixels for Stream to scroll horizontally
     * @param dy amount in pixels for Stream to scroll vertically
     */
    void smoothScrollBy(int dx, int dy);

    /**
     * Returns the top position in pixels of the View at the {@code position} in the vertical
     * hierarchy. This should act similarly to {@code RecyclerView.getChildAt(position).getTop()}.
     *
     * <p>Returns {@link #POSITION_NOT_KNOWN} if position is not known. This could be returned if
     * {@code position} it not a valid position or the actual child has not been placed on screen
     * and rendered.
     */
    int getChildTopAt(int position);

    /**
     * Returns true if the child at the position is visible on screen. The view could be partially
     * visible and this would still return true.
     */
    boolean isChildAtPositionVisible(int position);

    void addScrollListener(ScrollListener listener);

    void removeScrollListener(ScrollListener listener);

    void addOnContentChangedListener(ContentChangedListener listener);

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

    /**
     * Get whether the first card of Feed is dense in portrait mode.
     */
    default int getFirstCardDensity() {
        return FeedFirstCardDensity.UNKNOWN;
    }

    /**
     * Functions which report UMA / actions in Feed v2 only.
     */
    default void recordActionManageInterests() {}
    default void recordActionManageActivity() {}
    default void recordActionManageReactions() {}
    default void recordActionLearnMore() {}

    /**
     * Interface users can implement to know when content in the Stream has changed content on
     * screen.
     */
    interface ContentChangedListener {
        /**
         * Called by Stream when content being shown has changed. This could be new cards being
         * created, the content of a card changing, etc...
         */
        void onContentChanged();

        /**
         * Called by Stream when an
         * {@link androidx.recyclerview.widget.SimpleItemAnimator#onAddFinished} event is received.
         */
        default void onAddFinished(){};
    }
}
