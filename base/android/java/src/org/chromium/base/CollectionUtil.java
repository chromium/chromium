// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
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
    public static <E> ArrayList<E> newArrayList(E... elements) {
        ArrayList<E> list = new ArrayList<E>(elements.length);
        Collections.addAll(list, elements);
        return list;
    }

    @VisibleForTesting
    public static <E> ArrayList<E> newArrayList(Iterable<E> iterable) {
        ArrayList<E> list = new ArrayList<E>();
        for (E element : iterable) {
            list.add(element);
        }
        return list;
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
}
