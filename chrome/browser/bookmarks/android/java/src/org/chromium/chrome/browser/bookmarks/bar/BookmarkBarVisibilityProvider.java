// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
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
         * Called when the visibility state of the Bookmark Bar changes. Note: Only relevant when
         * the tri-state feature flag is enabled.
         *
         * @param visibilityState The new (now current) visibility state of the Bookmark Bar.
         */
        default void onVisibilityChanged_TriState(
                @BookmarkBarVisibilityState int visibilityState) {}

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
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private final ObserverList<BookmarkBarVisibilityObserver> mObservers;
    private final NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private final Callback<Boolean> mXrSpaceModeObserver = this::processXrSpaceModeChange;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable OnSharedPreferenceChangeListener mDevicePrefsListener;

    /**
     * Constructor.
     *
     * @param activity The activity in which the bookmark bar is hosted.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the host activity.
     * @param profileSupplier The supplier of the profile for which to observe the user setting.
     * @param xrSpaceModeObservableSupplier The supplier for the XR space mode state.
     */
    public BookmarkBarVisibilityProvider(
            Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;

        mObservers = new ObserverList<>();

        mConfigurationChangedListener = this::processConfigurationChange;
        mActivityLifecycleDispatcher.register(mConfigurationChangedListener);

        mProfileSupplierObserver = this::processProfileChange;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);

        mXrSpaceModeObservableSupplier.addSyncObserverAndPostIfNonNull(mXrSpaceModeObserver);

        // On tablets we use local device prefs.
        if (!DeviceInfo.isDesktop()) {
            // Depending on feature flag we use one of two different device preferences.
            String devicePrefKey =
                    ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)
                            ? BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE
                            : BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR;
            mDevicePrefsListener =
                    (sharedPreferences, key) -> {
                        if (key != null && key.equals(devicePrefKey)) {
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
        mXrSpaceModeObservableSupplier.removeObserver(mXrSpaceModeObserver);
        destroyPrefChangeRegistrar();
        destroySharedPrefListener();
        mObservers.clear();
    }

    private void notifyVisibilityChange() {
        // When the tri-state feature flag is not enabled, we use the v1 simple boolean.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)) {
            boolean visibility =
                    BookmarkBarUtils.isBookmarkBarVisible(
                            mActivity,
                            mProfileSupplier.get(),
                            mXrSpaceModeObservableSupplier.get());
            for (BookmarkBarVisibilityObserver observer : mObservers) {
                observer.onVisibilityChanged(visibility);
            }
            return;
        }

        @BookmarkBarVisibilityState
        int visibilityState =
                BookmarkBarUtils.getBookmarkBarVisibilityState(
                        mActivity, mProfileSupplier.get(), mXrSpaceModeObservableSupplier.get());
        for (BookmarkBarVisibilityObserver observer : mObservers) {
            observer.onVisibilityChanged_TriState(visibilityState);
        }
    }

    private void processXrSpaceModeChange(boolean isXrSpaceMode) {
        // When entering FSM for XR, browser UI must be manually hidden to show hub UI.
        notifyVisibilityChange();
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

    private void processProfileChange(Profile profile) {
        // On a profile change, we may have either received a profile for the first time, or we
        // have received a new profile, in which case we want to destroy the previous pref change
        // registrar and create a new one.
        destroyPrefChangeRegistrar();

        // Depending on feature flag we use one of two different UserPrefs.
        String profilePrefKey =
                ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)
                        ? Pref.BOOKMARK_BAR_VISIBILITY_STATE
                        : Pref.SHOW_BOOKMARK_BAR;

        mPrefChangeRegistrar = PrefServiceUtil.createFor(profile);
        mPrefChangeRegistrar.addObserver(profilePrefKey, this::processPrefChange);

        // Profile changes can also result in visibility changes (e.g. different setting prefs).
        notifyVisibilityChange();
    }

    private void processPrefChange() {
        // On any pref change, we need to notify all observers of visibility change.
        notifyVisibilityChange();
    }

    private void destroyPrefChangeRegistrar() {
        if (mPrefChangeRegistrar != null) {
            // Depending on feature flag we use one of two different UserPrefs.
            String profilePrefKey =
                    ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)
                            ? Pref.BOOKMARK_BAR_VISIBILITY_STATE
                            : Pref.SHOW_BOOKMARK_BAR;

            mPrefChangeRegistrar.removeObserver(profilePrefKey);
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
        return this::processPrefChange;
    }
}
