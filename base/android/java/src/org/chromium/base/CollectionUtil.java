// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.Pair;

import androidx.annotation.NonNull;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Functions used for easier initialization of Java collections. Inspired by
 * functionality in com.google.common.collect in Guava but cherry-picked to
 * bare-minimum functionality to avoid bloat. (http://crbug.com/272790 provides
 * further details)
 */
public final class CollectionUtil {
    private CollectionUtil() {}

    @SafeVarargs
    public static <E> HashSet<E> newHashSet(E... elements) {
        HashSet<E> set = new HashSet<E>(elements.length);
        Collections.addAll(set, elements);
        return set;
    }

    @SafeVarargs
    public static <K, V> HashMap<K, V> newHashMap(Pair<? extends K, ? extends V>... entries) {
        HashMap<K, V> map = new HashMap<>();
        for (Pair<? extends K, ? extends V> entry : entries) {
            map.put(entry.first, entry.second);
        }
        return map;
    }

    public static boolean[] booleanListToBooleanArray(@NonNull List<Boolean> list) {
        boolean[] array = new boolean[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }

    public static int[] integerListToIntArray(@NonNull List<Integer> list) {
        int[] array = new int[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }

    public static long[] longListToLongArray(@NonNull List<Long> list) {
        long[] array = new long[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }

    // This is a utility helper method that adds functionality available in API 24 (see
    // Collection.forEach).
    public static <T> void forEach(Collection<? extends T> collection, Callback<T> worker) {
        for (T entry : collection) worker.onResult(entry);
    }

    // This is a utility helper method that adds functionality available in API 24 (see
    // Collection.forEach).
    @SuppressWarnings("unchecked")
    public static <K, V> void forEach(
            Map<? extends K, ? extends V> map, Callback<Entry<K, V>> worker) {
        for (Map.Entry<? extends K, ? extends V> entry : map.entrySet()) {
            worker.onResult((Map.Entry<K, V>) entry);
        }
    }

    /**
     * Removes null entries from the given collection and then returns a list of strong references.
     *
     * Note: This helper is relevant if you have a List<WeakReference<T>> or a Map with weak values.
     * For Set<WeakReference<T>>, use Collections.newSetFromMap(new WeakHashMap()) instead.
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
}
