// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.os.Build;
import android.webkit.SafeBrowsingResponse;

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;

/** Chromium implementation of {@link SafeBrowsingResponse}. */
// TODO(ntfschr): remove @SuppressLint once lint uses 27 for targetSdk (this is needed to
// subclass SafeBrowsingResponse)
@SuppressLint({"Override"})
@RequiresApi(Build.VERSION_CODES.O_MR1)
public class SafeBrowsingResponseAdapter extends SafeBrowsingResponse {
    private final Callback<AwSafeBrowsingResponse> mCallback;

    public SafeBrowsingResponseAdapter(Callback<AwSafeBrowsingResponse> callback) {
        mCallback = callback;
    }

    /* package */ Callback<AwSafeBrowsingResponse> getAwSafeBrowsingResponseCallback() {
        return mCallback;
    }

    @Override
    public void showInterstitial(boolean allowReporting) {
        mCallback.onResult(
                new AwSafeBrowsingResponse(SafeBrowsingAction.SHOW_INTERSTITIAL, allowReporting));
    }

    @Override
    public void proceed(boolean report) {
        mCallback.onResult(new AwSafeBrowsingResponse(SafeBrowsingAction.PROCEED, report));
    }

    @Override
    public void backToSafety(boolean report) {
        mCallback.onResult(new AwSafeBrowsingResponse(SafeBrowsingAction.BACK_TO_SAFETY, report));
    }
}
