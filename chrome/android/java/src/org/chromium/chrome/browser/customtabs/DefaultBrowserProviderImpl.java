// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.pm.ResolveInfo;

import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;

import javax.inject.Inject;

/** Gets the default browser's package name. */
public class DefaultBrowserProviderImpl
        implements CustomTabActivityNavigationController.DefaultBrowserProvider {
    @Inject
    public DefaultBrowserProviderImpl() {}

    @Override
    public String getDefaultBrowser() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        if (info != null) {
            return info.activityInfo.packageName;
        }
        return null;
    }
}
