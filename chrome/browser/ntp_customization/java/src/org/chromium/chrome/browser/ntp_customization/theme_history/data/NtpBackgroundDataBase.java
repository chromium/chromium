// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Base class for NTP background data. */
@NullMarked
public abstract class NtpBackgroundDataBase {
    @VisibleForTesting static final String PLATFORM_TYPE_KEY = "platformType";
    @VisibleForTesting static final String BACKGROUND_TYPE_KEY = "backgroundType";
    @VisibleForTesting static final String PRIMARY_COLOR_KEY = "primaryColor";
    @VisibleForTesting static final String PORTRAIT_MATRIX_KEY = "portraitMatrix";
    @VisibleForTesting static final String LANDSCAPE_MATRIX_KEY = "landscapeMatrix";

    @IntDef({
        PlatformType.UNKNOWN,
        PlatformType.ANDROID_REMOTE,
        PlatformType.ANDROID_LOCAL,
        PlatformType.IOS,
        PlatformType.DESKTOP,
        PlatformType.ANDROID_DESKTOP,
        PlatformType.MAX_COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PlatformType {
        int UNKNOWN = 0;
        int ANDROID_LOCAL = 1;
        int ANDROID_REMOTE = 2;
        int IOS = 3;
        int DESKTOP = 4;
        int ANDROID_DESKTOP = 5;
        int MAX_COUNT = 6;
    }

    private final @PlatformType int mPlatformType;

    protected NtpBackgroundDataBase(@PlatformType int platformType) {
        mPlatformType = platformType;
    }

    /** Returns the platform type. */
    public @PlatformType int getPlatformType() {
        return mPlatformType;
    }

    /** Returns the NTP background type. */
    protected abstract @NtpBackgroundType int getBackgroundType();

    /** Returns the JSON representation of the object. */
    public JSONObject toJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put(PLATFORM_TYPE_KEY, mPlatformType);
        json.put(BACKGROUND_TYPE_KEY, getBackgroundType());
        return json;
    }
}
