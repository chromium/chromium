// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** The view for the auxiliary search module. */
public class AuxiliarySearchModuleView extends LinearLayout {

    public AuxiliarySearchModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    void setModuleButtonOnClickListener(@NonNull View.OnClickListener onClickListener) {}
}
