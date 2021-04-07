// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Holds a single page info.
 */
public class PageItem {
    private GURL mUrl;
    private String mTitle;

    public PageItem(GURL url, String title) {
        mUrl = url;
        mTitle = title;
    }

    /**
     * @return url of the result.
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * @return title of the result.
     */
    public String getTitle() {
        return mTitle;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof PageItem)) return false;

        PageItem other = (PageItem) o;

        return mUrl.equals(other.mUrl) && mTitle.equals(other.mTitle);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mUrl, mTitle);
    }
}
