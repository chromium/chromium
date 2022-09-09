// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

/**
 * A provider notifies all registered {@link Observer}s about a changed object.
 * @param <T> The object this provider provides.
 */
public interface Provider<T> {
    /**
     * Every observer added by this needs to be notified whenever the object changes.
     * @param observer The observer to be notified.
     */
    void addObserver(Observer<T> observer);

    /**
     * Passes the given item to all subscribed {@link Observer}s.
     * @param item The item to be passed to the {@link Observer}s.
     */
    void notifyObservers(T item);

    /**
     * An observer receives notifications from an {@link Provider} it is subscribed to.
     * @param <T> Any object that this instance observes.
     */
    interface Observer<T> {
        int DEFAULT_TYPE = Integer.MIN_VALUE;

        /**
         * A provider calls this function with an item that should be available in the keyboard
         * accessory.
         * @param typeId Specifies which type of item this update affects.
         * @param item An item to be displayed in the Accessory.
         */
        void onItemAvailable(int typeId, T item);
    }
}
