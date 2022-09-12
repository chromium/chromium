// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;

/** See {@link ChromeBrowserProviderImpl}. */
public class ChromeBrowserProvider extends SplitCompatContentProvider {
    @IdentifierNameString
    private static String sImplClassName =
            "org.chromium.chrome.browser.provider.ChromeBrowserProviderImpl";

    public ChromeBrowserProvider() {
        super(sImplClassName);
    }
}
