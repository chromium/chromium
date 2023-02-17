// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.annotation.NonNull;

import java.util.HashSet;
import java.util.Set;

/** Converts from {@link Bundle} to {@link PersistableBundle}. */
class BundleToPersistableBundleConverter {
    /**
     * A Result which contains the resulting {@link PersistableBundle} after a conversion, and also
     * contains a set of all the failed keys.
     */
    static class Result {
        @NonNull
        private final PersistableBundle mBundle;
        @NonNull
        private final Set<String> mFailedKeys;

        private Result(@NonNull PersistableBundle bundle, @NonNull Set<String> failedKeys) {
            mBundle = bundle;
            mFailedKeys = failedKeys;
        }

        boolean hasErrors() {
            return mFailedKeys.size() > 0;
        }

        @NonNull
        PersistableBundle getPersistableBundle() {
            return mBundle;
        }

        @NonNull
        Set<String> getFailedKeys() {
            return mFailedKeys;
        }

        String getFailedKeysErrorString() {
            StringBuilder sb = new StringBuilder();
            sb.append("{");
            boolean first = true;
            for (String key : mFailedKeys) {
                if (!first) sb.append(", ");
                first = false;

                sb.append(key);
            }
            sb.append("}");
            return sb.toString();
        }
    }

    /**
     * Copy all entries in the {@link Bundle} that can be part of a {@link PersistableBundle}.
     *
     * @param bundle the {@link Bundle} to convert.
     * @return a result object contain the resulting {@link PersistableBundle} and whether any of
     *         the keys failed.
     */
    static Result convert(Bundle bundle) {
        PersistableBundle persistableBundle = new PersistableBundle();
        Set<String> failedKeys = new HashSet<>();
        for (String key : bundle.keySet()) {
            Object obj = bundle.get(key);
            if (obj == null) {
                persistableBundle.putString(key, null);
            } else if (obj instanceof Boolean) {
                persistableBundle.putBoolean(key, (Boolean) obj);
            } else if (obj instanceof boolean[]) {
                persistableBundle.putBooleanArray(key, (boolean[]) obj);
            } else if (obj instanceof Double) {
                persistableBundle.putDouble(key, (Double) obj);
            } else if (obj instanceof double[]) {
                persistableBundle.putDoubleArray(key, (double[]) obj);
            } else if (obj instanceof Integer) {
                persistableBundle.putInt(key, (Integer) obj);
            } else if (obj instanceof int[]) {
                persistableBundle.putIntArray(key, (int[]) obj);
            } else if (obj instanceof Long) {
                persistableBundle.putLong(key, (Long) obj);
            } else if (obj instanceof long[]) {
                persistableBundle.putLongArray(key, (long[]) obj);
            } else if (obj instanceof String) {
                persistableBundle.putString(key, (String) obj);
            } else if (obj instanceof String[]) {
                persistableBundle.putStringArray(key, (String[]) obj);
            } else {
                failedKeys.add(key);
            }
        }
        return new Result(persistableBundle, failedKeys);
    }
}
