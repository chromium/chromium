// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import org.chromium.chrome.browser.base.SplitCompatContentProvider;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link ChromeBrowserProviderImpl}. */
public class ChromeBrowserProvider extends SplitCompatContentProvider {
    public ChromeBrowserProvider() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.provider.ChromeBrowserProviderImpl"));
    }
}
