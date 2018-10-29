// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.Log;
import org.chromium.base.process_launcher.ChildProcessService.SplitApkWorkaroundResult;

import java.io.File;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Map;

/**
 * WebView-side workaround for the Android O framework bug described in https://crbug.com/889954
 * which affects us if we are the current WebView provider and we were installed as a split APK.
 */
public class SplitApkWorkaround {
    private static final String TAG = "SplitApkWorkaround";

    /**
     * There is a framework bug in O that causes an incorrect classloader cache entry to be created
     * when the WebView provider is installed as multiple split APKs.
     * Use reflection to correct the cache entry during WebView zygote startup.
     * This function runs in the WebView zygote, which cannot make any binder calls to the framework
     * and is a very restricted environment.
     *
     * @param realRun If false, don't actually change any state in the framework; just verify that
     *               the reflection succeeds.
     * @return a value from Result describing what happened.
     */
    @SuppressWarnings("unchecked")
    public static @SplitApkWorkaroundResult int apply(boolean realRun) {
        int matchingEntries = 0;
        int exceptionEntries = 0;
        try {
            // Retrieve all the required classes and fields first, such that if any of the lookups
            // fail, we won't have done anything yet.
            Class<?> alClass = Class.forName("android.app.ApplicationLoaders");
            Method getDefaultMethod = alClass.getDeclaredMethod("getDefault");
            Field mLoadersField = alClass.getDeclaredField("mLoaders");
            mLoadersField.setAccessible(true);
            Field pathListField = BaseDexClassLoader.class.getDeclaredField("pathList");
            pathListField.setAccessible(true);
            Class<?> dplClass = Class.forName("dalvik.system.DexPathList");
            Field dexElementsField = dplClass.getDeclaredField("dexElements");
            dexElementsField.setAccessible(true);
            Class<?> elClass = Class.forName("dalvik.system.DexPathList$Element");
            Field pathField = elClass.getDeclaredField("path");
            pathField.setAccessible(true);

            // Retrieve the ApplicationLoaders singleton and get the cache from inside it.
            Object alInstance = getDefaultMethod.invoke(null);
            Object rawLoaders = mLoadersField.get(alInstance);
            Map<String, ClassLoader> loaders = (Map<String, ClassLoader>) rawLoaders;

            // Synchronize on the map while trying to update it, as the framework does.
            synchronized (loaders) {
                for (Map.Entry<String, ClassLoader> entry : loaders.entrySet()) {
                    try {
                        if (!(entry.getValue() instanceof BaseDexClassLoader)) {
                            // If it's some other type it can't be the right one.
                            continue;
                        }
                        String cacheKey = entry.getKey();
                        BaseDexClassLoader cl = (BaseDexClassLoader) entry.getValue();

                        // Get the list of files that this classloader uses as its classpath.
                        Object pathList = pathListField.get(cl);
                        Object dexElements = dexElementsField.get(pathList);

                        int elementCount = Array.getLength(dexElements);
                        if (elementCount <= 1) {
                            // If there's only one file, then this classloader cannot be affected by
                            // the bug, so ignore it.
                            continue;
                        }

                        // If there's more than one file, get the first file in the path.
                        // If it's the same as our cache key, then the cache key must, by
                        // definition, be incomplete - listing only one file when it should list
                        // all of them.
                        Object firstElement = Array.get(dexElements, 0);
                        File firstPath = (File) pathField.get(firstElement);
                        if (cacheKey.equals(firstPath.getPath())) {
                            // Build a new, correct cache key by concatenating all the files in the
                            // list together in order, separated by colons.
                            String newCacheKey = cacheKey;
                            for (int i = 1; i < elementCount; i++) {
                                Object element = Array.get(dexElements, i);
                                File path = (File) pathField.get(element);
                                newCacheKey += ":" + path.getPath();
                            }

                            matchingEntries++;
                            if (realRun) {
                                // Add a new entry to the cache which maps the new, correct key to
                                // the same classloader object. We do not remove the previous entry
                                // from the cache, in case something attempts to look it up by the
                                // old key for some reason - it shouldn't cause a problem for there
                                // to be multiple entries mapping to the same classloader.
                                loaders.put(newCacheKey, cl);
                                Log.i(TAG, "Fixed classloader cache entry for " + newCacheKey);
                            }
                        }
                    } catch (Exception e) {
                        // We log and ignore it here so we can continue looping through the cache,
                        // in the hope that the one that threw an exception wasn't the one we
                        // were looking for.
                        exceptionEntries++;
                        Log.w(TAG, "Caught exception while attempting to fix classloader cache", e);
                    }
                }
            }
        } catch (Exception e) {
            // If we got an exception at this point we assume that we failed to fix it, since we
            // didn't get as far as iterating over the cache entries.
            Log.w(TAG, "Caught exception while attempting to fix classloader cache", e);
            return SplitApkWorkaroundResult.TOPLEVEL_EXCEPTION;
        }

        // If we found at least one matching entry, then don't worry about exceptions that happened
        // during the loop; we likely found the correct classloader, and the one that triggered an
        // exception was probably not relevant. Distinguish one vs multiple entries, though,
        // because multiple matches is unexpected (only one case in the code is supposed to create
        // this situation).
        if (matchingEntries == 1) return SplitApkWorkaroundResult.ONE_ENTRY;
        if (matchingEntries > 1) return SplitApkWorkaroundResult.MULTIPLE_ENTRIES;

        // If we didn't find any matching entries, but did get an exception during the loop, then
        // report this, as we might have taken the exception while trying to access the entry we
        // needed to fix.
        if (exceptionEntries > 0) return SplitApkWorkaroundResult.LOOP_EXCEPTION;

        // Otherwise, we just didn't find any entries at all, which is probably fine; not all
        // configurations actually trigger the bug.
        return SplitApkWorkaroundResult.NO_ENTRIES;
    }
}
