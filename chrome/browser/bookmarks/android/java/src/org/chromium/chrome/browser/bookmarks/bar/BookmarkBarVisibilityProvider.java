// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A provider which observes changes to device configuration state and the bookmark bar user setting
 * in order to propagate visibility change events.
 */
@NullMarked
public class BookmarkBarVisibilityProvider {

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Callback<Boolean> mCallback;
    private final ConfigurationChangedObserver mConfigurationChangedListener;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final BookmarkBarSettingProvider mSettingProvider;
    private final ObservableSupplierImpl<Boolean> mVisibilitySupplier;

    /**
     * Constructor.
     *
     * @param activity The activity in which the bookmark bar is hosted.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the host activity.
     * @param profileSupplier The supplier of the profile for which to observe the user setting.
     * @param callback The callback to notify of changes to visibility.
     */
    public BookmarkBarVisibilityProvider(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull Callback<Boolean> callback) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mCallback = callback;
        mProfileSupplier = profileSupplier;

        mVisibilitySupplier = new ObservableSupplierImpl<>();
        mVisibilitySupplier.addObserver(mCallback);

        mConfigurationChangedListener = (unused) -> updateVisibility();
        mActivityLifecycleDispatcher.register(mConfigurationChangedListener);

        mSettingProvider =
                new BookmarkBarSettingProvider(
                        mProfileSupplier, /* callback= */ (unused) -> updateVisibility());
    }

    /** Destroys the visibility provider. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mConfigurationChangedListener);
        mSettingProvider.destroy();
        mVisibilitySupplier.removeObserver(mCallback);
    }

    private void updateVisibility() {
        mVisibilitySupplier.set(
                BookmarkBarUtils.isFeatureVisible(mActivity, mProfileSupplier.get()));
    }
}
