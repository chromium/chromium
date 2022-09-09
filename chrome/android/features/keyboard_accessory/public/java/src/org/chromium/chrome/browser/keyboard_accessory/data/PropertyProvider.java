// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import java.util.HashSet;
import java.util.Set;

/**
 * A simple class that holds a set of {@link Observer}s which can be notified about new data by
 * directly passing that data into {@link PropertyProvider#notifyObservers(T)}.
 * @param <T> The object this provider provides.
 */
public class PropertyProvider<T> implements Provider<T> {
    private final Set<Observer<T>> mObservers = new HashSet<>();
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
