// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/** A factory interface for building a {@link TabModelSelector} instance. */
public interface TabModelSelectorFactory {
    /**
     * Builds a {@link TabModelSelector}.
     *
     * @param context An {@link Context} instance.
     * @param profileProviderSupplier Provides the Profiles used in this selector.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param nextTabPolicySupplier A {@link NextTabPolicySupplier} instance.
     * @return A new {@link TabModelSelector} instance.
     */
    TabModelSelector buildSelector(
            Context context,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier);
}
