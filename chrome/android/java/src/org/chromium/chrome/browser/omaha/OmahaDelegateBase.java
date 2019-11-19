// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.pm.ApplicationInfo;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.AppHooks;

import java.util.UUID;

/** Delegates calls out from the OmahaClient. */
public abstract class OmahaDelegateBase extends OmahaDelegate {
    private final ExponentialBackoffScheduler mScheduler;
    private final Context mContext;

    OmahaDelegateBase(Context context) {
        mContext = context;
        mScheduler = new ExponentialBackoffScheduler(OmahaBase.PREF_PACKAGE, context,
                OmahaBase.MS_POST_BASE_DELAY, OmahaBase.MS_POST_MAX_DELAY);
    }

    @Override
    Context getContext() {
        return mContext;
    }

    @Override
    boolean isInSystemImage() {
        return (getContext().getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }

    @Override
    ExponentialBackoffScheduler getScheduler() {
        return mScheduler;
    }

    @Override
    String generateUUID() {
        return UUID.randomUUID().toString();
    }

    @Override
    boolean isChromeBeingUsed() {
        boolean isChromeVisible = ApplicationStatus.hasVisibleActivities();
        boolean isScreenOn = ApiCompatibilityUtils.isInteractive();
        return isChromeVisible && isScreenOn;
    }

    @Override
    protected RequestGenerator createRequestGenerator(Context context) {
        return AppHooks.get().createOmahaRequestGenerator();
    }
}
