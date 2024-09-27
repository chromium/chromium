// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_callback_glue;

import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.support_lib_boundary.SafeBrowsingResponseBoundaryInterface;

/**
 * Adapter between {@link Callback accepting} an {@link AwSafeBrowsingResponse} and {@link
 * SafeBrowsingResponseBoundaryInterface}.
 */
public class SupportLibSafeBrowsingResponse implements SafeBrowsingResponseBoundaryInterface {
    private final Callback<AwSafeBrowsingResponse> mCallback;

    public SupportLibSafeBrowsingResponse(Callback<AwSafeBrowsingResponse> callback) {
        mCallback = callback;
    }

    public Callback<AwSafeBrowsingResponse> getAwSafeBrowsingResponseCallback() {
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
