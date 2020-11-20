// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.ContextWrapper;
import android.view.LayoutInflater;

import androidx.collection.ArraySet;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.IdentifierNameString;

/** Utils for compatibility with isolated splits. */
public class SplitCompatUtils {
    private static final ArraySet<ClassLoader> sInflationClassLoaders = new ArraySet<>();

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
        return BundleUtils.createIsolatedSplitContext(base, "chrome");
    }

    /**
     * Constructs a new instance of the given class name using the class loader from the context.
     */
    public static Object newInstance(Context context, String className) {
        // TODO(crbug.com/1142589): If this crash fix works we can remove the context arg from here.
        Context appContext = ContextUtils.getApplicationContext();
        if (appContext != null) {
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
        ClassLoader splitClassLoader =
                BundleUtils.createIsolatedSplitContext(context, splitName).getClassLoader();
        // All Contexts for a split share a ClassLoader, so the maximum size of this set will be the
        // number of installed splits.
        sInflationClassLoaders.add(splitClassLoader);
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

    /**
     * Returns a FragmentFactory which can load fragment classes from any split which an inflation
     * context has been created for. This is useful if a fragment lives in an isolated split and is
     * not retained. It may be recreated on configuration changes, and will need to be loaded from
     * the correct ClassLoader.
     */
    public static FragmentFactory createFragmentFactory() {
        return new FragmentFactory() {
            @Override
            public Fragment instantiate(ClassLoader classLoader, String className) {
                if (!canLoadClass(classLoader, className)) {
                    for (ClassLoader cl : sInflationClassLoaders) {
                        if (canLoadClass(cl, className)) {
                            classLoader = cl;
                            break;
                        }
                    }
                }
                return super.instantiate(classLoader, className);
            }
        };
    }

    private static boolean canLoadClass(ClassLoader classLoader, String className) {
        try {
            Class.forName(className, false, classLoader);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }
}
