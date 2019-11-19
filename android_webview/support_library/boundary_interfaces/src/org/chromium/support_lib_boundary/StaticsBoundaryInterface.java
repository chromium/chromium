// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.Context;
import android.net.Uri;
import android.webkit.ValueCallback;

import java.util.List;

/**
 * Boundary interface for WebViewFactoryProvider.Statics.
 */
public interface StaticsBoundaryInterface {
    void initSafeBrowsing(Context context, ValueCallback<Boolean> callback);
    void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback);
    Uri getSafeBrowsingPrivacyPolicyUrl();
    boolean isMultiProcessEnabled();
}
