// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
/**
 * View for the feed header that sticks to the top of the screen upon scroll.
 */
public class StickySectionHeaderView extends SectionHeaderView {
    public StickySectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    void setStickyHeaderVisible(boolean visible) {}
}
