// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import androidx.annotation.DrawableRes;

/**
 * Public version of {@link TouchToFillResourceProviderImpl}.
 * Downstream could provide a different implementation.
 */
class TouchToFillResourceProviderImpl implements TouchToFillResourceProvider {
    @Override
    public @DrawableRes int getHeaderImageDrawableId() {
        return R.drawable.touch_to_fill_header_image;
    }
}
