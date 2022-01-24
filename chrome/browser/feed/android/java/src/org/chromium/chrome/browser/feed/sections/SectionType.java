// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({SectionType.FOR_YOU_FEED, SectionType.WEB_FEED})
@Retention(RetentionPolicy.SOURCE)
public @interface SectionType {
    int FOR_YOU_FEED = 0;
    int WEB_FEED = 1;
}
