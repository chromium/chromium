// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * Centralizes backward-compatibility quirks across WebView.
 *
 * <p>All quirks represent deviations from modern/standard behavior for backward compatibility with
 * older target SDK levels or Android CompatChanges. A return value of {@code true} means the legacy
 * compatibility quirk should be applied, while {@code false} represents the standard modern
 * behavior.
 */
@NullMarked
public abstract class CompatQuirks {
    @IntDef({
        Quirk.ALLOW_SNIFFING_FILE_URLS,
        Quirk.DATA_DIRECTORY_LOCK_WARN_ONLY,
        Quirk.FIXUP_OCTOTHORPES_IN_LOAD_DATA,
        Quirk.ALLOW_FILE_URL_ACCESS_BY_DEFAULT,
        Quirk.LEGACY_DARK_MODE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Quirk {
        /** Allows MIME-type sniffing for file:// URLs. Normally enabled for apps targeting < P. */
        int ALLOW_SNIFFING_FILE_URLS = 0;

        /**
         * Log a warning instead of crashing if locking the data directory fails. Normally enabled
         * for apps targeting < P.
         */
        int DATA_DIRECTORY_LOCK_WARN_ONLY = 1;

        /**
         * Fixes up unencoded '#' characters in data: URLs in WebView.loadData(). Normally enabled
         * for apps targeting < Q.
         */
        int FIXUP_OCTOTHORPES_IN_LOAD_DATA = 2;

        /** Allow loading file:// URLs by default. Normally enabled for apps targeting < R. */
        int ALLOW_FILE_URL_ACCESS_BY_DEFAULT = 3;

        /**
         * Uses legacy dark mode logic instead of modern simplified dark mode. Normally enabled for
         * apps targeting < T.
         */
        int LEGACY_DARK_MODE = 4;
    }

    /**
     * Delegate interface implemented by the embedder or glue layer to determine whether
     * backward-compatibility quirks are enabled (e.g. based on target SDK or CompatChanges).
     */
    public interface Delegate {
        boolean isEnabled(@Quirk int quirk);
    }

    private static @Nullable Delegate sDelegate;
    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static final Map<Integer, Boolean> sTestOverrides = new HashMap<>();

    /** Sets the delegate for querying backward-compatibility quirks. */
    public static void setDelegate(Delegate delegate) {
        assert sDelegate == null;
        sDelegate = delegate;
    }

    /**
     * Returns whether the given backward-compatibility quirk is enabled.
     *
     * @param quirk The quirk identifier from {@link Quirk}.
     * @return {@code true} if the legacy compatibility quirk is enabled; {@code false} for modern
     *     default behavior.
     */
    public static boolean isEnabled(@Quirk int quirk) {
        synchronized (sLock) {
            Boolean override = sTestOverrides.get(quirk);
            if (override != null) {
                return override;
            }
        }
        if (sDelegate == null) {
            return false;
        }
        return sDelegate.isEnabled(quirk);
    }

    public static void overrideForTesting(@Quirk int quirk, boolean enabled) {
        synchronized (sLock) {
            sTestOverrides.put(quirk, enabled);
        }
    }

    public static void resetForTesting() {
        synchronized (sLock) {
            sTestOverrides.clear();
        }
    }
}
