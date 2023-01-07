// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Specifies recognizable search types.
 */
@IntDef({SearchType.TEXT, SearchType.VOICE, SearchType.LENS})
@Retention(RetentionPolicy.SOURCE)
/* package */ @interface SearchType {
    int TEXT = 0;
    int VOICE = 1;
    int LENS = 2;
}
