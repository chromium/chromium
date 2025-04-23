// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.FlowType;

/** Creates a {@link CollaborationControllerDelegateImpl} for {@link DataSharingTabManager}. */
@FunctionalInterface
@NullMarked
public interface CollaborationControllerDelegateFactory {
    /**
     * Returns a {@link CollaborationControllerDelegate}.
     *
     * @param type The flow type of the new flow.
     * @param switchToTabSwitcherCallback The callback to switch to tab switcher view.
     */
    CollaborationControllerDelegate create(
            @FlowType int type, @Nullable Callback<Runnable> switchToTabSwitcherCallback);
}
