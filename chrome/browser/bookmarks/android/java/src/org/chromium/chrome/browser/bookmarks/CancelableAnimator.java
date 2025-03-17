// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

/**
 * Semi-tag interface that allows {@link RecyclerView.Adapter} to cast {@link View} to, and clear
 * out in progress animations. This should hopefully clear the {@link View#hasTransientState()},
 * allowing the {@link RecyclerView} to recycle the {@link View}. Implements are allowed to be a
 * best effort, and if no in progress animation is running this call should no-op.
 */
interface CancelableAnimator {
    void cancelAnimation();
}
