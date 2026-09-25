// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.PageState;

import java.util.function.BiFunction;

/** Represents a Page and is exposed to embedders. See also AwNavigationListener */
@NullMarked
public class AwPage extends AwSupportLibIsomorphic {
    private final Page mPage;
    private final BiFunction<AwPage, PageState, @Nullable AwPageState> mPageStateProvider;

    public AwPage(
            Page page, BiFunction<AwPage, PageState, @Nullable AwPageState> pageStateProvider) {
        mPage = page;
        mPageStateProvider = pageStateProvider;
    }

    public String getUrl() {
        return mPage.getUrl().getSpec();
    }

    public @Nullable AwPageState snapshotState() {
        return mPageStateProvider.apply(this, getMostRecentPageState());
    }

    @AnyThread
    public PageState getMostRecentPageState() {
        return mPage.getMostRecentPageState();
    }

    public Page getInternalPageForTesting() {
        return mPage;
    }
}
