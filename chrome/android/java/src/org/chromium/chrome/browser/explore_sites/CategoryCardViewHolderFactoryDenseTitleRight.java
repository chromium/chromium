// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import org.chromium.chrome.R;

/** CategoryCardViewHolderFactory for Dense Title Right variation. */
public class CategoryCardViewHolderFactoryDenseTitleRight extends CategoryCardViewHolderFactory {
    @Override
    protected int getTileViewResource() {
        return R.layout.explore_sites_dense_tile_right_view;
    }

    @Override
    protected int getCategoryCardViewResource() {
        return R.layout.explore_sites_dense_category_card_view;
    }
}
