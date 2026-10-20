// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.Profile;

import org.chromium.build.annotations.NullMarked;

/**
 * Profile-specific Header Interceptor handle, which allows embedders to unregister an interceptor.
 */
@NullMarked
/* package */ class SupportLibProfileHeaderInterceptorHandle implements Runnable {
    private final Profile mProfile;
    private final int mId;

    public SupportLibProfileHeaderInterceptorHandle(Profile profile, int id) {
        mProfile = profile;
        mId = id;
    }

    @Override
    public void run() {
        mProfile.getBrowserContext().getHeaderInterceptorStore().removeInterceptor(mId);
    }
}
