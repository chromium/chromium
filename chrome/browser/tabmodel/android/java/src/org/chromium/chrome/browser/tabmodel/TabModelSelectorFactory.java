// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * A factory interface for building a {@link TabModelSelector} instance.
 */
public interface TabModelSelectorFactory {
    /**
     * Builds a {@link TabModelSelector}.
     *
     * @param activity An {@link Activity} instance.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param nextTabPolicySupplier A {@link NextTabPolicySupplier} instance.
     * @param selectorIndex The index of the {@link TabModelSelector}.
     * @return A new {@link TabModelSelector} instance.
     */
    TabModelSelector buildSelector(Activity activity, TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex);
}
