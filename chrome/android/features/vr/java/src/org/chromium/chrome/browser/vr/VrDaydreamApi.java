// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

import com.google.vr.ndk.base.DaydreamApi;
import com.google.vr.ndk.base.GvrApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

/** A wrapper for DaydreamApi. */
public class VrDaydreamApi {
    private DaydreamApi mDaydreamApi;

    private DaydreamApi getDaydreamApi() {
        if (mDaydreamApi == null) {
            mDaydreamApi = DaydreamApi.create(ContextUtils.getApplicationContext());
        }
        return mDaydreamApi;
    }

    public boolean registerDaydreamIntent(final PendingIntent pendingIntent) {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.registerDaydreamIntent(pendingIntent);
        return true;
    }

    public boolean unregisterDaydreamIntent() {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.unregisterDaydreamIntent();
        return true;
    }

    public boolean launchInVr(final PendingIntent pendingIntent) {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.launchInVr(pendingIntent);
        return true;
    }

    public boolean launchInVr(final Intent intent) {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.launchInVr(intent);
        return true;
    }

    public boolean exitFromVr(Activity activity, int requestCode, final Intent intent) {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.exitFromVr(activity, requestCode, intent);
        return true;
    }

    public boolean isDaydreamCurrentViewer() {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        int type = GvrApi.ViewerType.CARDBOARD;
        // If this is the first time any app reads the daydream config file, daydream may create its
        // config directory... crbug.com/686104
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            type = daydreamApi.getCurrentViewerType();
        } catch (RuntimeException ex) {
            // TODO(mthiesse, b/110092501): Remove this exception handling once Daydream handles
            // this exception.
            // We occasionally get the hidden CursorWindowAllocationException here, presumably from
            // being OOM when trying to check the current viewer type.
            return false;
        }
        return type == GvrApi.ViewerType.DAYDREAM;
    }

    public boolean launchVrHomescreen() {
        DaydreamApi daydreamApi = getDaydreamApi();
        if (daydreamApi == null) return false;
        daydreamApi.launchVrHomescreen();
        return true;
    }

    public void close() {
        if (mDaydreamApi == null) return;
        mDaydreamApi.close();
        mDaydreamApi = null;
    }
}
