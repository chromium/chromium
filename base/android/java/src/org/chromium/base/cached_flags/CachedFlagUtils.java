// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import java.util.List;

/**
 * Utility methods for {@link CachedFlag}.
 *
 * <p>TODO(crbug.com/40266922): Rename this to CachedFlagUtils.
 */
public class CachedFlagUtils {
    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheNativeFlags(List<CachedFlag> featuresToCache) {
        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (CachedFlag feature : featuresToCache) {
            feature.writeCacheValueToEditor(editor);
        }
        editor.apply();
    }

    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheFieldTrialParameters(List<CachedFieldTrialParameter> parameters) {
        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (final CachedFieldTrialParameter parameter : parameters) {
            parameter.writeCacheValueToEditor(editor);
        }
        editor.apply();
    }

    /**
     * Do not call this from tests.
     *
     * <p>Features.JUnitProcessor and Features.InstrumentationProcessor already reset this state.
     *
     * <p>Exceptions are tests that test the flags infrastructure.
     */
    public static void resetFlagsForTesting() {
        ValuesReturned.clearForTesting();
        ValuesOverridden.removeOverrides();
    }
}
