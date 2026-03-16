// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Helper class to return values associated with flags for tab state storage functionality. */
@NullMarked
public final class TabStateStorageFlagHelper {
    public static final String DEFAULT_PHASE = "";
    public static final String PHASE_ONLY_SHADOW = "only_shadow";
    public static final String PHASE_AUTHORITATIVE_READ_SOURCE = "authoritative_read_source";
    public static final String PHASE_FULL_MIGRATION = "full_migration";
    public static final String PHASE_FULL_ROLLBACK = "full_rollback";

    private TabStateStorageFlagHelper() {}

    /** Returns whether tab state storage functionality is enabled. */
    public static boolean isTabStorageEnabled() {
        return ChromeFeatureList.sTabStorageSqlitePrototype.isEnabled();
    }

    /** Returns whether tab state store is only shadowing the legacy store. */
    public static boolean onlyShadow() {
        String phase = ChromeFeatureList.sTabStorageSqlitePrototypePhase.getValue();

        // By default, if the feature is enabled, Tab State Store is enabled in shadow mode.
        boolean onlyShadow = phase.equals(PHASE_ONLY_SHADOW) || phase.equals(DEFAULT_PHASE);
        if (onlyShadow) {
            return true;
        }
        assertValidPhase(phase);
        return false;
    }

    /** Returns whether tab state store is authoritative as the source of truth. */
    public static boolean isStorageAuthoritative() {
        String phase = ChromeFeatureList.sTabStorageSqlitePrototypePhase.getValue();
        boolean authoritative =
                phase.equals(PHASE_AUTHORITATIVE_READ_SOURCE) || phase.equals(PHASE_FULL_MIGRATION);
        if (authoritative) {
            return true;
        }
        assertValidPhase(phase);
        return false;
    }

    /**
     * Returns whether tab state store is authoritative as the source of truth and the legacy store
     * is not required to shadow.
     */
    public static boolean allowFullMigration() {
        String phase = ChromeFeatureList.sTabStorageSqlitePrototypePhase.getValue();
        boolean fullMigration = phase.equals(PHASE_FULL_MIGRATION);
        if (fullMigration) {
            return true;
        }
        assertValidPhase(phase);
        return false;
    }

    /**
     * Returns whether the tab state store is now disabled, but a catch up is required to roll back
     * to the legacy store.
     */
    public static boolean fullRollback() {
        String phase = ChromeFeatureList.sTabStorageSqlitePrototypePhase.getValue();
        boolean fullRollback = phase.equals(PHASE_FULL_ROLLBACK);
        if (fullRollback) {
            return true;
        }
        assertValidPhase(phase);
        return false;
    }

    public static void assertValidPhase(String phase) {
        assert phase.equals(DEFAULT_PHASE)
                || phase.equals(PHASE_ONLY_SHADOW)
                || phase.equals(PHASE_AUTHORITATIVE_READ_SOURCE)
                || phase.equals(PHASE_FULL_MIGRATION)
                || phase.equals(PHASE_FULL_ROLLBACK);
    }
}
