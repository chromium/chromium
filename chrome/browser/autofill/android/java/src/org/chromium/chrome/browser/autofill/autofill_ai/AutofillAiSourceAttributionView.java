// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;

/** Inflates and holds references to the Autofill AI source attribution bottom sheet views. */
@NullMarked
/*package*/ class AutofillAiSourceAttributionView {
    private final RecyclerView mRecyclerView;

    AutofillAiSourceAttributionView(Context context) {
        mRecyclerView =
                (RecyclerView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.autofill_ai_source_attribution_sheet, null);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
    }

    public View getContentView() {
        return mRecyclerView;
    }

    public void setAdapter(RecyclerView.Adapter<?> adapter) {
        mRecyclerView.setAdapter(adapter);
    }
}
