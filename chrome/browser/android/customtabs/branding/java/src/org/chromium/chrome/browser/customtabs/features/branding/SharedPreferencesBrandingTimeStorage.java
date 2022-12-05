// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.util.HashUtil;

/**
 * Shared preference that stores the last branding time for CCT client apps. When recording, the
 * instance uses {@link SharedPreferences.Editor#commit()}.
 */
class SharedPreferencesBrandingTimeStorage implements BrandingChecker.BrandingLaunchTimeStorage {
    private static final String KEY_SHARED_PREF = "pref_cct_brand_show_time";

    private static SharedPreferencesBrandingTimeStorage sInstance;

    /** Shared pref that must be read / write on background thread. */
    private SharedPreferences mSharedPref;

    private SharedPreferencesBrandingTimeStorage() {
        if (BrandingController.USE_TEMPORARY_STORAGE.getValue()) {
            resetSharedPref();
        }
    }

    static SharedPreferencesBrandingTimeStorage getInstance() {
        if (sInstance == null) {
            sInstance = new SharedPreferencesBrandingTimeStorage();
        }
        return sInstance;
    }

    @VisibleForTesting
    static void resetInstanceForTesting() {
        sInstance = null;
    }

    @WorkerThread
    @Override
    public long get(String packageName) {
        return getSharedPref().getLong(hash(packageName), BrandingChecker.BRANDING_TIME_NOT_FOUND);
    }

    @MainThread
    @Override
    @SuppressLint({"ApplySharedPref"})
    public void put(String packageName, long brandingLaunchTime) {
        PostTask.postTask(TaskTraits.USER_VISIBLE_MAY_BLOCK, () -> {
            getSharedPref().edit().putLong(hash(packageName), brandingLaunchTime).commit();
        });
    }

    /**
     * @return Size of the current shared preference. Should not run on main thread as we are
     * reading all the keys from disk.
     */
    @WorkerThread
    int getSize() {
        return getSharedPref().getAll().size();
    }

    @VisibleForTesting
    public void resetSharedPref() {
        getSharedPref().edit().clear().apply();
    }

    private String hash(String packageName) {
        return HashUtil.getMd5Hash(new HashUtil.Params(packageName));
    }

    private SharedPreferences getSharedPref() {
        if (mSharedPref == null) {
            mSharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    KEY_SHARED_PREF, Context.MODE_PRIVATE);
        }
        return mSharedPref;
    }
}
