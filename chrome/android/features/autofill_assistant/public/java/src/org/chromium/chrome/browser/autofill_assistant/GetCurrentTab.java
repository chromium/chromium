// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/** Return the activity's current tab or {@code null}. */
interface GetCurrentTab {
    @Nullable
    Tab get();
}
