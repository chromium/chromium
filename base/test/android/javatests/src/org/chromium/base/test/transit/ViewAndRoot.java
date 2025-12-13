// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.Root;

/** A data class for {@link View} and the {@link Root} where it was found. */
class ViewAndRoot {
    public final View view;
    public final Root root;

    ViewAndRoot(View view, Root root) {
        this.view = view;
        this.root = root;
    }
}
