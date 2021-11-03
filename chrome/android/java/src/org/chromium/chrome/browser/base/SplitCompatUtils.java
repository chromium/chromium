// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.Bundle;
import android.view.LayoutInflater;

import androidx.collection.ArrayMap;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.IdentifierNameString;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;

/** Utils for compatibility with isolated splits. */
public class SplitCompatUtils {
    public static final String CHROME_SPLIT_NAME = "chrome";
    private static final String LOADED_SPLITS_KEY = "split_compat_loaded_splits";
    private static final Map<String, ClassLoader> sInflationClassLoaders =
            Collections.synchronizedMap(new ArrayMap<>());
    private static SplitCompatClassLoader sSplitCompatClassLoaderInstance;
    // List of splits that were loaded during the last run of chrome when
    // restoring from recents.
    private static ArrayList<String> sSplitsToRestore;

    private SplitCompatUtils() {}

    /**
     * Gets the obfuscated name for the passed in class name. Important: this MUST be called with a
     * string literal, otherwise @IdentifierNameString will not work.
     */
    @IdentifierNameString
    public static String getIdentifierName(String className) {
        return className;
    }

    /** Creates a context which can be used to load code and resources in the chrome split. */
    public static Context createChromeContext(Context base) {
        if (!BundleUtils.isIsolatedSplitInstalled(base, CHROME_SPLIT_NAME)) {
            return base;
        }
        return BundleUtils.createIsolatedSplitContext(base, CHROME_SPLIT_NAME);
    }

    /**
     * Constructs a new instance of the given class name. If the application context class loader
     * can load the class, that class loader will be used, otherwise the class loader from the
     * passed in context will be used.
     */
    public static Object newInstance(Context context, String className) {
        Context appContext = ContextUtils.getApplicationContext();
        if (appContext != null && canLoadClass(appContext.getClassLoader(), className)) {
            context = appContext;
        }
        try {
            return context.getClassLoader().loadClass(className).newInstance();
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Creates a context which can access classes from the specified split, but inherits theme
     * resources from the passed in context. This is useful if a context is needed to inflate
     * layouts which reference classes from a split.
     */
    public static Context createContextForInflation(Context context, String splitName) {
        if (!BundleUtils.isIsolatedSplitInstalled(context, splitName)) {
            return context;
        }
        ClassLoader splitClassLoader = registerSplitClassLoaderForInflation(splitName);
        return new ContextWrapper(context) {
            @Override
            public ClassLoader getClassLoader() {
                return splitClassLoader;
            }

            @Override
            public Object getSystemService(String name) {
                Object ret = super.getSystemService(name);
                if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                    ret = ((LayoutInflater) ret).cloneInContext(this);
                }
                return ret;
            }
        };
    }

    public static ClassLoader registerSplitClassLoaderForInflation(String splitName) {
        ClassLoader splitClassLoader = sInflationClassLoaders.get(splitName);
        if (splitClassLoader == null) {
            splitClassLoader = BundleUtils
                                       .createIsolatedSplitContext(
                                               ContextUtils.getApplicationContext(), splitName)
                                       .getClassLoader();
            sInflationClassLoaders.put(splitName, splitClassLoader);
        }
        return splitClassLoader;
    }

    static boolean canLoadClass(ClassLoader classLoader, String className) {
        try {
            Class.forName(className, false, classLoader);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    public static ClassLoader getSplitCompatClassLoader() {
        // SplitCompatClassLoader needs to be lazy loaded to ensure the Chrome
        // context is loaded and its class loader is set as the parent
        // classloader for the SplitCompatClassLoader. This happens in
        // Application#attachBaseContext.
        if (sSplitCompatClassLoaderInstance == null) {
            sSplitCompatClassLoaderInstance = new SplitCompatClassLoader();
        }
        return sSplitCompatClassLoaderInstance;
    }

    public static void saveLoadedSplits(Bundle outState) {
        outState.putStringArrayList(
                LOADED_SPLITS_KEY, new ArrayList(sInflationClassLoaders.keySet()));
    }

    public static void restoreLoadedSplits(Bundle savedInstanceState) {
        if (savedInstanceState == null) {
            return;
        }
        sSplitsToRestore = savedInstanceState.getStringArrayList(LOADED_SPLITS_KEY);
    }

    private static class SplitCompatClassLoader extends ClassLoader {
        public SplitCompatClassLoader() {
            // The chrome split classloader if the chrome split exists, otherwise
            // the base module class loader.
            super(ContextUtils.getApplicationContext().getClassLoader());
        }

        private Class<?> checkSplitsClassLoaders(String className) throws ClassNotFoundException {
            for (ClassLoader cl : sInflationClassLoaders.values()) {
                try {
                    return cl.loadClass(className);
                } catch (ClassNotFoundException ignore) {
                }
            }
            return null;
        }

        /**
         * Loads the class with the specified binary name.
         */
        @Override
        public Class<?> findClass(String cn) throws ClassNotFoundException {
            Class<?> foundClass = checkSplitsClassLoaders(cn);
            if (foundClass != null) {
                return foundClass;
            }
            // We will never have android.* classes in isolated split class loaders,
            // but android framework inflater does sometimes try loading classes
            // that do not exist when inflating xml files on startup.
            if (!cn.startsWith("android.") && sSplitsToRestore != null) {
                // If we fail from all the currently loaded classLoaders, lets
                // try loading some splits that were loaded when chrome was last
                // run and check again.
                restoreSplitsClassLoaders();
                foundClass = checkSplitsClassLoaders(cn);
                if (foundClass != null) {
                    return foundClass;
                }
            }
            throw new ClassNotFoundException(cn);
        }

        private void restoreSplitsClassLoaders() {
            if (sSplitsToRestore == null) {
                return;
            }
            // Load splits that were stored in the SavedInstanceState Bundle.
            for (String splitName : sSplitsToRestore) {
                if (!sInflationClassLoaders.containsKey(splitName)) {
                    registerSplitClassLoaderForInflation(splitName);
                }
            }
            sSplitsToRestore = null;
        }
    }
}
