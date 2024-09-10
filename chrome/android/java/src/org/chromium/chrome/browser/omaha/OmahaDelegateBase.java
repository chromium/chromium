// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.PowerManager;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;

import java.util.UUID;

/** Delegates calls out from the OmahaClient. */
public abstract class OmahaDelegateBase extends OmahaDelegate {
    private final ExponentialBackoffScheduler mScheduler;

    OmahaDelegateBase() {
        mScheduler =
                new ExponentialBackoffScheduler(
                        OmahaPrefUtils.PREF_PACKAGE,
                        OmahaBase.MS_POST_BASE_DELAY,
                        OmahaBase.MS_POST_MAX_DELAY);
    }

    @Override
    boolean isInSystemImage() {
        return (ContextUtils.getApplicationContext().getApplicationInfo().flags
                        & ApplicationInfo.FLAG_SYSTEM)
                != 0;
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
        if (!ApplicationStatus.hasVisibleActivities()) return false;

        PowerManager powerManager =
                (PowerManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.POWER_SERVICE);
        return powerManager.isInteractive();
    }

    @Override
    protected @Nullable RequestGenerator createRequestGenerator() {
        return ServiceLoaderUtil.maybeCreate(RequestGenerator.class);
    }
}
