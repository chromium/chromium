// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;

/**
 * A provider which observes changes to device configuration state and the Bookmark Bar user setting
 * in order to propagate visibility change events. This class allows observers to get notifications
 * of changes in visibility in a Profile-agnostic way. This circumvents the need for clients to
 * explicitly observe both profile and preference change events, as well as the Configuration, in
 * order to track the current visibility state of the Bookmark Bar.
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
         * @param minWidth The new (now current) min width of a bookmark in the Bookmark Bar.
         * @param maxWidth The new (now current) max width of a bookmark in the Bookmark Bar.
         */
        default void onItemWidthConstraintsChanged(int minWidth, int maxWidth) {}
    }

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ConfigurationChangedObserver mConfigurationChangedListener;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private final ObserverList<BookmarkBarVisibilityObserver> mObservers;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable OnSharedPreferenceChangeListener mDevicePrefsListener;

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

        mProfileSupplierObserver = this::processProfileChange;
        mProfileSupplier.addObserver(mProfileSupplierObserver);

        // On tablets we use local device prefs.
        if (!DeviceInfo.isDesktop()) {
            mDevicePrefsListener =
                    (sharedPreferences, key) -> {
                        if (key != null
                                && key.equals(
                                        BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR)) {
                            processPrefChange();
                        }
                    };
            ContextUtils.getAppSharedPreferences()
                    .registerOnSharedPreferenceChangeListener(mDevicePrefsListener);
        }
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
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
        destroyPrefChangeRegistrar();
        destroySharedPrefListener();
        mObservers.clear();
    }

    private void notifyVisibilityChange() {
        boolean visibility =
                BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfileSupplier.get());
        for (BookmarkBarVisibilityObserver observer : mObservers) {
            observer.onVisibilityChanged(visibility);
        }
    }

    private void processConfigurationChange(Configuration configuration) {
        int minWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bookmark_bar_item_min_width);
        int maxWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width);
        for (BookmarkBarVisibilityObserver observer : mObservers) {
            observer.onItemWidthConstraintsChanged(minWidth, maxWidth);
        }

        // Configuration changes can also result in visibility changes (e.g. window size change).
        notifyVisibilityChange();
    }

    private void processProfileChange(@Nullable Profile profile) {
        // On a profile change, we may have either received a profile for the first time, or we
        // have received a new profile, in which case we want to destroy the previous pref change
        // registrar and create a new one.
        destroyPrefChangeRegistrar();

        if (profile != null) {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(profile);
            mPrefChangeRegistrar.addObserver(Pref.SHOW_BOOKMARK_BAR, this::processPrefChange);
        }

        // Profile changes can also result in visibility changes (e.g. different setting prefs).
        notifyVisibilityChange();
    }

    private void processPrefChange() {
        // On any pref change, we need to notify all observers of visibility change.
        notifyVisibilityChange();
    }

    private void destroyPrefChangeRegistrar() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.removeObserver(Pref.SHOW_BOOKMARK_BAR);
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
    }

    private void destroySharedPrefListener() {
        if (mDevicePrefsListener != null) {
            ContextUtils.getAppSharedPreferences()
                    .unregisterOnSharedPreferenceChangeListener(mDevicePrefsListener);
            mDevicePrefsListener = null;
        }
    }

    @Nullable PrefObserver getPrefObserverForTesting() {
        return new PrefObserver() {
            @Override
            public void onPreferenceChange() {
                processPrefChange();
            }
        };
    }
}
