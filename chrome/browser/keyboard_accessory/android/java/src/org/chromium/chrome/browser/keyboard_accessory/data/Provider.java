// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import org.chromium.build.annotations.NullMarked;

import java.util.HashSet;
import java.util.Set;

/**
 * A simple class that holds a set of {@link Observer}s which can be notified about new data by
 * directly passing that data into {@link Provider#notifyObservers(T)}.
 *
 * @param <T> The object this provider provides.
 */
@NullMarked
public class Provider<T> {
    /**
     * An observer receives notifications from a {@link Provider} it is subscribed to.
     *
     * @param <T> Any object that this instance observes.
     */
    public interface Observer<T> {
        int DEFAULT_TYPE = Integer.MIN_VALUE;

        /**
         * A provider calls this function with an item that should be available in the keyboard
         * accessory.
         *
         * @param typeId Specifies which type of item this update affects.
         * @param item An item to be displayed in the Accessory.
         */
        void onItemAvailable(int typeId, T item);
    }

    private final Set<Observer<T>> mObservers = new HashSet<>();
    protected int mType;

    public Provider() {
        this(Observer.DEFAULT_TYPE);
    }

    public Provider(int type) {
        mType = type;
    }

    public void addObserver(Observer<T> observer) {
        mObservers.add(observer);
    }

    public void notifyObservers(T item) {
        for (Observer<T> observer : mObservers) {
            observer.onItemAvailable(mType, item);
        }
    }
}
