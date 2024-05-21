// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.Log;

import java.util.HashMap;
import java.util.Map;

/** Checks if any restrictions exist and skip the test if it meets those restrictions. */
public final class RestrictionSkipCheck extends SkipCheck {
    public interface RestrictionHandler {
        boolean shouldSkip();
    }

    private static final String TAG = "RestrictionSkipCheck";

    private final Map<String, RestrictionHandler> mRestrictionHandlers = new HashMap<>();

    public void addHandler(String restrictionValue, RestrictionHandler handler) {
        mRestrictionHandlers.put(restrictionValue, handler);
    }

    @Override
    public boolean shouldSkip(FrameworkMethod frameworkMethod) {
        if (frameworkMethod == null) return true;

        for (Restriction restriction :
                AnnotationProcessingUtils.getAnnotations(
                        frameworkMethod.getMethod(), Restriction.class)) {
            for (String restrictionVal : restriction.value()) {
                RestrictionHandler handler = mRestrictionHandlers.get(restrictionVal);
                if (handler == null) {
                    throw new IllegalStateException(
                            "Unknown value for @Restriction: "
                                    + restrictionVal
                                    + "\nDid you perhaps use the wrong @RunWith?");
                }
                if (handler.shouldSkip()) {
                    Log.i(
                            TAG,
                            "Test %s#%s skipped because of restriction %s",
                            frameworkMethod.getDeclaringClass().getName(),
                            frameworkMethod.getName(),
                            restriction);
                    return true;
                }
            }
        }
        return false;
    }
}
