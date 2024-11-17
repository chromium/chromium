// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import androidx.annotation.DrawableRes;

/**
 * Provides functions that choose the correct resource id for touch-to-fill UI. Needed to
 * differentiate upstream and downstream resources. This exists to ensure all implementations of
 * TouchToFillResourceProviderImpl provide the same set of methods.
 *
 * <p>TODO(wnwen): Remove this once downstream no longer depends on it.
 */
interface TouchToFillResourceProvider {
    /**
     * Returns the drawable id to be displayed as a bottom sheet header image.
     *
     * @return A {@link DrawableRes} that is never 0.
     */
    public @DrawableRes int getHeaderImageDrawableId();
}
