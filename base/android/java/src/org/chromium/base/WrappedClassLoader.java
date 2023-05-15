// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import dalvik.system.BaseDexClassLoader;

/**
 * This class wraps two given ClassLoader objects and delegates findClass() and findLibrary() calls
 * to the first one that returns a match.
 */
public class WrappedClassLoader extends ClassLoader {
    private final ClassLoader mPrimaryClassLoader;
    private final ClassLoader mSecondaryClassLoader;

    public WrappedClassLoader(ClassLoader primary, ClassLoader secondary) {
        mPrimaryClassLoader = primary;
        mSecondaryClassLoader = secondary;
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            return mPrimaryClassLoader.loadClass(name);
        } catch (ClassNotFoundException e) {
            try {
                return mSecondaryClassLoader.loadClass(name);
            } catch (ClassNotFoundException e2) {
                e.addSuppressed(e2);
                throw e;
            }
        }
    }

    @Override
    public String findLibrary(String name) {
        String path = null;
        // BaseDexClassLoader has a public findLibrary method, but ClassLoader's is protected
        // so we can only do this for classloaders that actually do extend BaseDexClassLoader.
        // findLibrary is rarely used so it's fine to just check this each time.
        if (mPrimaryClassLoader instanceof BaseDexClassLoader) {
            path = ((BaseDexClassLoader) mPrimaryClassLoader).findLibrary(name);
            if (path != null) return path;
        }
        if (mSecondaryClassLoader instanceof BaseDexClassLoader) {
            path = ((BaseDexClassLoader) mSecondaryClassLoader).findLibrary(name);
        }
        return path;
    }
}
