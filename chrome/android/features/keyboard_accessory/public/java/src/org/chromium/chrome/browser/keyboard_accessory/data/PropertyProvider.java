// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import java.util.ArrayList;
import java.util.List;

/**
 * A simple class that holds a list of {@link Observer}s which can be notified about new data by
 * directly passing that data into {@link PropertyProvider#notifyObservers(T)}.
 * @param <T> The object this provider provides.
 */
public class PropertyProvider<T> implements Provider<T> {
    private final List<Observer<T>> mObservers = new ArrayList<>();
    protected int mType;

    public PropertyProvider() {
        this(Observer.DEFAULT_TYPE);
    }

    public PropertyProvider(int type) {
        mType = type;
    }

    @Override
    public void addObserver(Observer<T> observer) {
        mObservers.add(observer);
    }

    @Override
    public void notifyObservers(T item) {
        for (Observer<T> observer : mObservers) {
            observer.onItemAvailable(mType, item);
        }
    }
}
