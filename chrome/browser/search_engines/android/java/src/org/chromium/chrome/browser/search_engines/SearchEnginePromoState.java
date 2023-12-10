// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The current state regarding search engine promo dialogs. */
@IntDef({
    SearchEnginePromoState.SHOULD_CHECK,
    SearchEnginePromoState.CHECKED_NOT_SHOWN,
    SearchEnginePromoState.CHECKED_AND_SHOWN
})
@Retention(RetentionPolicy.SOURCE)
public @interface SearchEnginePromoState {
    int SHOULD_CHECK = -1;
    int CHECKED_NOT_SHOWN = 0;
    int CHECKED_AND_SHOWN = 1;
}
