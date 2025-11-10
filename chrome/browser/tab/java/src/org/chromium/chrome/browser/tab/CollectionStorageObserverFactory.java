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

/**
 * Java Factory for CollectionStorageObserver. This class facilitates propagating updates to the
 * structure of a TabStripCollection to storage.
 */
@NullMarked
@JNINamespace("tabs")
public class CollectionStorageObserverFactory {
    public final Profile profile;

    public CollectionStorageObserverFactory(Profile profile) {
        this.profile = profile;
    }

    @CalledByNative
    public static long build(CollectionStorageObserverFactory factory) {
        return CollectionStorageObserverFactoryJni.get().build(factory.profile);
    }

    @NativeMethods
    interface Natives {
        long build(@JniType("Profile*") Profile profile);
    }
}
