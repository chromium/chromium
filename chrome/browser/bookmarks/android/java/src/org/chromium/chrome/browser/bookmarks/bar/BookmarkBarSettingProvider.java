// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;

/**
 * A provider which observes and propagates events on changes to the bookmark bar user setting in a
 * profile-agnostic way. This circumvents the need for clients to explicitly observe both profile
 * and preference change events in order to track the current state of the bookmark bar user
 * setting.
 */
@NullMarked
public class BookmarkBarSettingProvider {

    private final Callback<Boolean> mCallback;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private final ObservableSupplierImpl<Boolean> mSettingSupplier;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

    /**
     * Constructor.
     *
     * @param profile The profile for which to observe the user setting.
     * @param callback The callback to notify of changes to the user setting.
     */
    public BookmarkBarSettingProvider(Profile profile, Callback<Boolean> callback) {
        this(new ObservableSupplierImpl<>(profile), callback);
    }

    /**
     * Constructor.
     *
     * @param profileSupplier The supplier of the profile for which to observe the user setting.
     * @param callback The callback to notify of changes to the user setting.
     */
    public BookmarkBarSettingProvider(
            ObservableSupplier<Profile> profileSupplier, Callback<Boolean> callback) {
        mCallback = callback;

        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = this::onProfileChange;
        mProfileSupplier.addObserver(mProfileSupplierObserver);

        mSettingSupplier = new ObservableSupplierImpl<>();
        mSettingSupplier.addObserver(mCallback);
    }

    /** Destroys the setting provider. */
    public void destroy() {
        destroyPrefChangeRegistrar();
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
        mSettingSupplier.removeObserver(mCallback);
    }

    private void onProfileChange(@Nullable Profile profile) {
        destroyPrefChangeRegistrar();

        if (profile != null) {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(profile);
            BookmarkBarUtils.addSettingObserver(mPrefChangeRegistrar, this::updateSetting);
        }

        updateSetting();
    }

    private void destroyPrefChangeRegistrar() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
    }

    private void updateSetting() {
        mSettingSupplier.set(BookmarkBarUtils.isSettingEnabled(mProfileSupplier.get()));
    }
}
