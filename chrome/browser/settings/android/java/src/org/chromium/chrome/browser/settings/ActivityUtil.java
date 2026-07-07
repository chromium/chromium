// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility methods for Activities in settings. */
@NullMarked
public final class ActivityUtil {
    private ActivityUtil() {}

    /**
     * Returns the Activity associated with the given context, unwrapping ContextWrapper if needed.
     *
     * @param context The context to lookup the Activity.
     * @return The Activity associated with the context, or null if not found.
     */
    public static @Nullable Activity getActivityFromContext(@Nullable Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }
}
