// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/**
 * Java Factory for CollectionStorageObserver. This class facilitates propagating updates to the
 * structure of a TabStripCollection to storage.
 */
@NullMarked
@JNINamespace("tabs")
public class CollectionStorageObserverFactory {
    public final Profile profile;
    public final TabStripCollection collection;

    /**
     * Constructs a factory for {@link CollectionStorageObserver}.
     *
     * @param profile The profile associated with the storage.
     * @param collection The tab strip collection being observed.
     */
    public CollectionStorageObserverFactory(Profile profile, TabStripCollection collection) {
        this.profile = profile;
        this.collection = collection;
    }

    @CalledByNative
    public static long build(CollectionStorageObserverFactory factory) {
        return CollectionStorageObserverFactoryJni.get().build(factory.profile, factory.collection);
    }

    @NativeMethods
    interface Natives {
        long build(
                @JniType("Profile*") Profile profile,
                @JniType("tabs::TabStripCollection*") TabStripCollection collection);
    }
}
