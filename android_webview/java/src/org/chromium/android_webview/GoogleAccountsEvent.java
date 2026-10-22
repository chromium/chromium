// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.GAIAServiceType;

/**
 * Event containing parameters from a Google Accounts header request. Corresponds to
 * ManageAccountsParams in C++.
 */
@NullMarked
public class GoogleAccountsEvent {
    private final @GAIAServiceType int mServiceType;
    private final String mContinueUrl;
    private final boolean mIsOutermostMainFrame;
    private final boolean mIsNavigationRequest;

    public GoogleAccountsEvent(
            @GAIAServiceType int serviceType,
            String continueUrl,
            boolean isOutermostMainFrame,
            boolean isNavigationRequest) {
        mServiceType = serviceType;
        mContinueUrl = continueUrl;
        mIsOutermostMainFrame = isOutermostMainFrame;
        mIsNavigationRequest = isNavigationRequest;
    }

    public @GAIAServiceType int getServiceType() {
        return mServiceType;
    }

    public String getContinueUrl() {
        return mContinueUrl;
    }

    public boolean isOutermostMainFrame() {
        return mIsOutermostMainFrame;
    }

    public boolean isNavigationRequest() {
        return mIsNavigationRequest;
    }
}
