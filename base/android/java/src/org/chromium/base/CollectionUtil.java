// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/** Helpers for working with collections that do not already exist in JDK / Guava. */
@NullMarked
public final class CollectionUtil {
    private CollectionUtil() {}

    public static int[] integerCollectionToIntArray(Collection<Integer> collection) {
        int[] array = new int[collection.size()];
        int index = 0;
        for (int num : collection) {
            array[index] = num;
            index++;
        }
        return array;
    }

    /**
     * Removes null entries from the given collection and then returns a list of strong references.
     *
     * <p>Note: This helper is relevant if you have a List<WeakReference<T>> or a Map with weak
     * values. For Set<WeakReference<T>>, use Collections.newSetFromMap(new WeakHashMap()) instead.
     *
     * @param weakRefs Collection to iterate.
     * @return List of strong references.
     */
    public static <T> List<T> strengthen(Collection<WeakReference<T>> weakRefs) {
        ArrayList<T> ret = new ArrayList<>(weakRefs.size());
        Iterator<WeakReference<T>> it = weakRefs.iterator();
        while (it.hasNext()) {
            WeakReference<T> weakRef = it.next();
            T strongRef = weakRef.get();
            if (strongRef == null) {
                it.remove();
            } else {
                ret.add(strongRef);
            }
        }
        return ret;
    }

    /** Flattens a collection of collections. */
    public static <T> List<T> flatten(Collection<? extends Collection<T>> input) {
        List<T> ret = new ArrayList<>();
        for (Collection<T> inner : input) {
            ret.addAll(inner);
        }
        return ret;
    }
}
