// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;

/** Feature flags computed from system capabilities. */
public final class AwComputedFlags {
    // Do not instantiate this class.
    private AwComputedFlags() {}

    private static final String GMS_PACKAGE = "com.google.android.gms";

    private static Boolean sPageStartedOnCommitForBrowserNavigations;

    private static boolean computePageStartedOnCommitForBrowserNavigations() {
        if (GMS_PACKAGE.equals(ContextUtils.getApplicationContext().getPackageName())) {
            int gmsPackageVersion = PackageUtils.getPackageVersion(GMS_PACKAGE);
            return gmsPackageVersion >= 15000000;
        }
        return true;
    }

    public static boolean pageStartedOnCommitEnabled(boolean isRendererInitiated) {
        // Always enable for renderer-initiated navigations.
        if (isRendererInitiated) return true;
        if (sPageStartedOnCommitForBrowserNavigations != null) {
            return sPageStartedOnCommitForBrowserNavigations;
        }
        sPageStartedOnCommitForBrowserNavigations =
                computePageStartedOnCommitForBrowserNavigations();
        return sPageStartedOnCommitForBrowserNavigations;
    }
}
