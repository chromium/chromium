// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public interface CommerceBottomSheetContentController {
    /** Request to show the commerce bottom sheet. */
    void requestShowContent();
}
