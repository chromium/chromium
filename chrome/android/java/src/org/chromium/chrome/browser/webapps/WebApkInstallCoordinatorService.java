// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link WebApkInstallCoordinatorServiceImpl}. */
public class WebApkInstallCoordinatorService extends SplitCompatService {
    @IdentifierNameString
    private static String sImplClassName =
            "org.chromium.chrome.browser.webapps.WebApkInstallCoordinatorServiceImpl";

    public WebApkInstallCoordinatorService() {
        super(sImplClassName);
    }
}
