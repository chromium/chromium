// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.content.Context;
import android.util.AttributeSet;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class PreferenceListRecyclerView extends RecyclerView {
    public PreferenceListRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        setLayoutManager(layoutManager);
        DividerItemDecoration divider =
                new DividerItemDecoration(context, layoutManager.getOrientation());
        addItemDecoration(divider);
    }
}
