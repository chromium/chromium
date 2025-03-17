// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchDonor.SetDocumentClassVisibilityForPackageCallback;

/** Provides access to internal AuxiliarySearch implementation parts, if they are available. */
public interface AuxiliarySearchHooks {
    /** Whether the internal components of the Auxiliary Search are available. */
    boolean isEnabled();

    /**
     * Sets the schema visibility for the requestBuilder.
     *
     * @param callback The callback to set the document visibilities.
     */
    default void setSchemaTypeVisibilityForPackage(
            @NonNull SetDocumentClassVisibilityForPackageCallback callback) {}

    /** Returns whether the sharing Tabs with the system is enabled by default on the device. */
    default boolean isSettingDefaultEnabledByOs() {
        return false;
    }

    /** Returns the package name of the supported app which reads the donated Tabs. */
    @Nullable
    default String getSupportedPackageName() {
        return null;
    }
}
