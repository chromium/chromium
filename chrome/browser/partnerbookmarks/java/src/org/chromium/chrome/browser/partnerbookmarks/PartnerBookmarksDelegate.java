// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import androidx.annotation.Nullable;

/**
 * Base class for defining methods where different behavior is required by downstream targets.
 * The correct version of {@link PartnerBookmarksDelegateImpl} will be determined at compile time
 * via build rules.
 */
public interface PartnerBookmarksDelegate {
    @Nullable
    PartnerBookmarkIterator createIterator();
}
