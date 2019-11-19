// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.support.v4.content.ContextCompat;

import org.chromium.android.support.PackageManagerWrapper;
import org.chromium.base.Log;
import org.chromium.base.annotations.MainDex;

import java.io.File;
import java.io.IOException;
import java.io.Serializable;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.Comparator;

/**
 *  Functionality common to the JUnit3 and JUnit4 runners.
 */
@MainDex
class BaseChromiumRunnerCommon {
    private static final String TAG = "base_test";

    /**
     *  A ContextWrapper that allows multidex test APKs to extract secondary dexes into
     *  the APK under test's data directory.
     */
    @MainDex
    static class MultiDexContextWrapper extends ContextWrapper {
        private final Context mAppContext;

        MultiDexContextWrapper(Context instrContext, Context appContext) {
            super(instrContext);
            mAppContext = appContext;
        }

        @Override
        public File getFilesDir() {
            return mAppContext.getFilesDir();
        }

        @Override
        public SharedPreferences getSharedPreferences(String name, int mode) {
            // Prefix so as to not conflict with main app's multidex prefs file.
            return mAppContext.getSharedPreferences("test-" + name, mode);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public ApplicationInfo getApplicationInfo(String packageName, int flags) {
                    try {
                        ApplicationInfo ai = super.getApplicationInfo(packageName, flags);
                        if (packageName.equals(getPackageName())) {
                            File dataDir = new File(
                                    ContextCompat.getCodeCacheDir(mAppContext), "test-multidex");
                            if (!dataDir.exists() && !dataDir.mkdirs()) {
                                throw new IOException(String.format(
                                        "Unable to create test multidex directory \"%s\"",
                                        dataDir.getPath()));
                            }
                            ai.dataDir = dataDir.getPath();
                        }
                        return ai;
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to get application info for %s", packageName, e);
                    }
                    return null;
                }
            };
        }
    }

    /**
     * Ensure all test dex entries precede app dex entries.
     *
     * @param cl ClassLoader to modify. Assumed to be a derivative of
     *        {@link dalvik.system.BaseDexClassLoader}. If this isn't
     *        the case, reordering will fail.
     */
    static void reorderDexPathElements(ClassLoader cl, Context context, Context targetContext) {
        try {
            Log.i(TAG,
                    "Reordering dex files. If you're building a multidex test APK and see a "
                            + "class resolving to an unexpected implementation, this may be why.");
            Field pathListField = findField(cl, "pathList");
            Object dexPathList = pathListField.get(cl);
            Field dexElementsField = findField(dexPathList, "dexElements");
            Object[] dexElementsList = (Object[]) dexElementsField.get(dexPathList);
            Arrays.sort(dexElementsList,
                    new DexListReorderingComparator(
                            context.getPackageName(), targetContext.getPackageName()));
            dexElementsField.set(dexPathList, dexElementsList);
        } catch (Exception e) {
            Log.e(TAG, "Failed to reorder dex elements for testing.", e);
        }
    }

    /**
     *  Comparator for sorting dex list entries.
     *
     *  Using this to sort a list of dex list entries will result in the following order:
     *   - Strings that contain neither the test package nor the app package in lexicographical
     *     order.
     *   - Strings that contain the test package in lexicographical order.
     *   - Strings that contain the app package but not the test package in lexicographical order.
     */
    private static class DexListReorderingComparator implements Comparator<Object>, Serializable {
        private String mTestPackage;
        private String mAppPackage;

        public DexListReorderingComparator(String testPackage, String appPackage) {
            mTestPackage = testPackage;
            mAppPackage = appPackage;
        }

        @Override
        public int compare(Object o1, Object o2) {
            String s1 = o1.toString();
            String s2 = o2.toString();
            if (s1.contains(mTestPackage)) {
                if (!s2.contains(mTestPackage)) {
                    if (s2.contains(mAppPackage)) {
                        return -1;
                    } else {
                        return 1;
                    }
                }
            } else if (s1.contains(mAppPackage)) {
                if (s2.contains(mTestPackage)) {
                    return 1;
                } else if (!s2.contains(mAppPackage)) {
                    return 1;
                }
            } else if (s2.contains(mTestPackage) || s2.contains(mAppPackage)) {
                return -1;
            }
            return s1.compareTo(s2);
        }
    }

    private static Field findField(Object instance, String name) throws NoSuchFieldException {
        for (Class<?> clazz = instance.getClass(); clazz != null; clazz = clazz.getSuperclass()) {
            try {
                Field f = clazz.getDeclaredField(name);
                f.setAccessible(true);
                return f;
            } catch (NoSuchFieldException e) {
            }
        }
        throw new NoSuchFieldException(
                "Unable to find field " + name + " in " + instance.getClass());
    }
}
