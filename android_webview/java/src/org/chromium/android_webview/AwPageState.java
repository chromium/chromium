// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.PageState;

/** Represents a PageState and is exposed to embedders. See also AwNavigationListener */
@AnyThread
@NullMarked
public class AwPageState {

    private final PageState mPageState;
    private final AwPage mAwPage;

    public AwPageState(PageState pageState, AwPage awPage) {
        mPageState = pageState;
        mAwPage = awPage;
    }

    public String getUrl() {
        return mPageState.getUrl().getSpec();
    }

    public AwPage getPage() {
        return mAwPage;
    }
}
