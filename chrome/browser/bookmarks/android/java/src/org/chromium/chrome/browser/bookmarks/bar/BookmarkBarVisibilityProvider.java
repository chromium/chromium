// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.res.Configuration;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A provider which observes changes to device configuration state and the bookmark bar user setting
 * in order to propagate visibility change events.
 */
@NullMarked
public class BookmarkBarVisibilityProvider {

    /**
     * Interface to define an observer of visibility changes for the Bookmark Bar. The visibility
     * can change from user setting or device configuration changes.
     */
    public interface BookmarkBarVisibilityObserver {
        /**
         * Called when the visibility of the Bookmark Bar changes.
         *
         * @param visibility The new (now current) visibility of the Bookmark Bar.
         */
        default void onVisibilityChanged(boolean visibility) {}

        /**
         * Called when the max width of a bookmark in the Bookmark Bar changes based on the
         * configuration of the device.
         *
         * @param maxWidth The new (now current) max width of a bookmark in the Bookmark Bar.
         */
        default void onMaxWidthChanged(int maxWidth) {}
    }

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ConfigurationChangedObserver mConfigurationChangedListener;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final BookmarkBarSettingProvider mSettingProvider;
    private final ObserverList<BookmarkBarVisibilityObserver> mObservers;

    /**
     * Constructor.
     *
     * @param activity The activity in which the bookmark bar is hosted.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the host activity.
     * @param profileSupplier The supplier of the profile for which to observe the user setting.
     */
    public BookmarkBarVisibilityProvider(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<Profile> profileSupplier) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mProfileSupplier = profileSupplier;

        mObservers = new ObserverList<>();

        mConfigurationChangedListener = this::processConfigurationChange;
        mActivityLifecycleDispatcher.register(mConfigurationChangedListener);

        mSettingProvider =
                new BookmarkBarSettingProvider(mProfileSupplier, this::processSettingChange);
    }

    /**
     * Adds the given observer to |this| to receive notifications of visibility changes.
     *
     * @param observer The observer to add to the observer list of |this|.
     */
    public void addObserver(BookmarkBarVisibilityObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes the given observer from |this| to no longer receive notifications of visibility
     * changes.
     *
     * @param observer The observer to remove from the observer list of |this|.
     */
    public void removeObserver(BookmarkBarVisibilityObserver observer) {
        mObservers.removeObserver(observer);
    }

    /** Destroys the visibility provider. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mConfigurationChangedListener);
        mSettingProvider.destroy();
        mObservers.clear();
    }

    private void processSettingChange(Boolean isSettingEnabled) {
        boolean visibility = BookmarkBarUtils.isFeatureVisible(mActivity, mProfileSupplier.get());
        for (BookmarkBarVisibilityObserver observer : mObservers) {
            observer.onVisibilityChanged(visibility);
        }
    }

    private void processConfigurationChange(Configuration configuration) {
        int maxWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width);
        for (BookmarkBarVisibilityObserver observer : mObservers) {
            observer.onMaxWidthChanged(maxWidth);
        }

        // Configuration changes can also result in visibility changes (e.g. window size change).
        processSettingChange(BookmarkBarUtils.isSettingEnabled(mProfileSupplier.get()));
    }
}
