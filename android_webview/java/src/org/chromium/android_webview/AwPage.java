// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.Page;

/** Represents a Page and is exposed to embedders. See also AwNavigationListener */
@NullMarked
public class AwPage extends AwSupportLibIsomorphic {
    private final Page mPage;

    public AwPage(Page page) {
        mPage = page;
    }

    public String getUrl() {
        return mPage.getUrl().getSpec();
    }

    public Page getInternalPageForTesting() {
        return mPage;
    }
}
