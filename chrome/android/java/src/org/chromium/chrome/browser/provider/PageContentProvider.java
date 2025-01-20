// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;

/** See {@link PageContentProviderImpl}. */
public class PageContentProvider extends SplitCompatContentProvider {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.provider.PageContentProviderImpl";

    public PageContentProvider() {
        super(sImplClassName);
    }
}
