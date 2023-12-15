// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.util.HashUtil;

import java.util.Map;

/**
 * Shared preference that stores the last branding time for CCT client apps. When recording, the
 * instance uses {@link SharedPreferences.Editor#commit()}.
 */
class SharedPreferencesBrandingTimeStorage implements BrandingChecker.BrandingLaunchTimeStorage {
    private static final String KEY_SHARED_PREF = "pref_cct_brand_show_time";
    private static final String NON_PACKAGE_PREFIX = "REFERRER_";
    @VisibleForTesting static final int MAX_NON_PACKAGE_ENTRIES = 50;

    private static SharedPreferencesBrandingTimeStorage sInstance;

    /** Shared pref that must be read / write on background thread. */
    private SharedPreferences mSharedPref;

    private SharedPreferencesBrandingTimeStorage() {}

    static SharedPreferencesBrandingTimeStorage getInstance() {
        if (sInstance == null) {
            sInstance = new SharedPreferencesBrandingTimeStorage();
        }
        return sInstance;
    }

    static void resetInstance() {
        sInstance = null;
    }

    @WorkerThread
    @Override
    public long get(String appId) {
        return getSharedPref().getLong(getKey(appId), BrandingChecker.BRANDING_TIME_NOT_FOUND);
    }

    @MainThread
    @Override
    @SuppressLint({"ApplySharedPref"})
    public void put(String appId, long brandingLaunchTime) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> {
                    SharedPreferences.Editor pref = getSharedPref().edit();
                    String entry = getOldEntryToTrim();
                    if (entry != null) pref.remove(entry);
                    pref.putLong(getKey(appId), brandingLaunchTime);
                    pref.commit();
                });
    }

    private String getKey(String appId) {
        assert !TextUtils.isEmpty(appId);
        String key = hash(appId);
        // Keys will have a prefix if they are not a valid package name. They are likely
        // to be numerous, therefore the number of such entries will be kept under a
        // given threshold. The prefix makes it easy to do the job.
        if (!PackageUtils.isPackageInstalled(appId)) key = NON_PACKAGE_PREFIX + key;
        return key;
    }

    @WorkerThread
    private String getOldEntryToTrim() {
        String oldEntry = null;
        long oldTime = -1;
        int count = 0;
        for (Map.Entry<String, ?> entry : getSharedPref().getAll().entrySet()) {
            String key = entry.getKey();
            if (!key.startsWith(NON_PACKAGE_PREFIX)) continue;
            count++;
            long time = (long) entry.getValue();
            if (oldTime < 0 || time < oldTime) {
                oldEntry = entry.getKey();
                oldTime = time;
            }
        }
        return count >= MAX_NON_PACKAGE_ENTRIES ? oldEntry : null;
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
            mSharedPref =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(KEY_SHARED_PREF, Context.MODE_PRIVATE);
        }
        return mSharedPref;
    }

    void setSharedPrefForTesting(SharedPreferences pref) {
        mSharedPref = pref;
    }
}
