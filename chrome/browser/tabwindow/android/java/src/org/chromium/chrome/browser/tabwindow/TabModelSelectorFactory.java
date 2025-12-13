// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A factory interface for building a {@link TabModelSelector} instance. */
@NullMarked
public interface TabModelSelectorFactory {
    /**
     * Builds a {@link TabModelSelector}.
     *
     * @param context An {@link Context} instance.
     * @param modalDialogManager The {@link ModalDialogManager}.
     * @param profileProviderSupplier Provides the Profiles used in this selector.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param nextTabPolicySupplier A {@link NextTabPolicySupplier} instance.
     * @param multiInstanceManager A {@link MultiInstanceManager} instance.
     * @return A new {@link TabModelSelector} instance.
     */
    TabModelSelector buildTabbedSelector(
            Context context,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MultiInstanceManager multiInstanceManager);

    /**
     * Builds and initializes the tab model. Outside infra should ensure that this is the exclusive
     * user of the given window id.
     *
     * @param windowId Used to decide what files to load.
     * @param profile The current regular profile.
     * @return The created tab model selector and a mechanism to shut it down.
     */
    Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
            @WindowId int windowId, Profile profile);
}
