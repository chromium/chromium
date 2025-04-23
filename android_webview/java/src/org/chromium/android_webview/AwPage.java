// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.Page;

/** Represents a Page and is exposed to embedders. See also AwNavigationClient */
@NullMarked
public class AwPage extends AwSupportLibIsomorphic {
    private final Page mPage;

    public AwPage(Page page) {
        mPage = page;
    }

    public boolean isPrerendering() {
        return mPage.isPrerendering();
    }

    public Page getInternalPageForTesting() {
        return mPage;
    }
}
