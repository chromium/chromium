// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import dalvik.system.BaseDexClassLoader;

/**
 * This class wraps two given BaseDexClassLoader's and delegates findClass() and findLibrary() calls
 * to the first one that returns a match.
 */
public class WrappedClassLoader extends ClassLoader {
    private BaseDexClassLoader mPrimaryClassLoader;
    private BaseDexClassLoader mSecondaryClassLoader;

    public WrappedClassLoader(BaseDexClassLoader primary, BaseDexClassLoader secondary) {
        this.mPrimaryClassLoader = primary;
        this.mSecondaryClassLoader = secondary;
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            return mPrimaryClassLoader.loadClass(name);
        } catch (ClassNotFoundException e) {
            return mSecondaryClassLoader.loadClass(name);
        }
    }

    @Override
    public String findLibrary(String name) {
        String path = mPrimaryClassLoader.findLibrary(name);
        if (path != null) return path;

        return mSecondaryClassLoader.findLibrary(name);
    }
}
