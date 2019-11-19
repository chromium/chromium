// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

/**
 * A {@link ListObserver} that calls {@link #onDataSetChanged} when any item of the list is
 * inserted, removed or update.
 * @param <T> the type of the observed list.
 */
public abstract class AbstractListObserver<T> implements ListObserver<T> {
    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        onDataSetChanged();
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        onDataSetChanged();
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<T> source, int index, int count, @Nullable T payload) {
        onDataSetChanged();
    }

    public abstract void onDataSetChanged();
}
