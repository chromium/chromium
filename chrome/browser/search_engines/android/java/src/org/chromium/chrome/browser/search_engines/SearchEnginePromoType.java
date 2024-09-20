// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The different types of search engine promo dialogs. */
@IntDef({
    SearchEnginePromoType.DONT_SHOW,
    SearchEnginePromoType.SHOW_SOGOU,
    SearchEnginePromoType.SHOW_EXISTING,
    SearchEnginePromoType.SHOW_NEW,
})
@Retention(RetentionPolicy.SOURCE)
public @interface SearchEnginePromoType {
    int DONT_SHOW = -1;
    int SHOW_SOGOU = 0;
    int SHOW_EXISTING = 1;
    int SHOW_NEW = 2;
}
