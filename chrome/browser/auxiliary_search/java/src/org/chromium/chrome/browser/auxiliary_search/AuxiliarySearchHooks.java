// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchDonor.SetDocumentClassVisibilityForPackageCallback;

/** Provides access to internal AuxiliarySearch implementation parts, if they are available. */
@NullMarked
public interface AuxiliarySearchHooks {
    /** Whether the internal components of the Auxiliary Search are available. */
    boolean isEnabled();

    /**
     * Sets the schema visibility for the requestBuilder.
     *
     * @param callback The callback to set the document visibilities.
     */
    default void setSchemaTypeVisibilityForPackage(
            SetDocumentClassVisibilityForPackageCallback callback) {}

    /** Returns whether the sharing Tabs with the system is enabled by default on the device. */
    default boolean isSettingDefaultEnabledByOs() {
        return false;
    }

    /** Returns whether multiple data source is enabled on the device. */
    default boolean isMultiDataTypeEnabledOnDevice() {
        return false;
    }

    /** Returns the package name of the supported app which reads the donated Tabs. */
    default @Nullable String getSupportedPackageName() {
        return null;
    }
}
