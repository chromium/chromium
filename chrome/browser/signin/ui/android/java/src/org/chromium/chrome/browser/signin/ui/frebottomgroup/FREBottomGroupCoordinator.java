// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.frebottomgroup;

import android.content.Context;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator handles the update and interaction of the bottom group of the FRE sign-in
 * screen. It is composed of a selected account, a continue button and a dismiss button.
 */
public class FREBottomGroupCoordinator {
    /**
     * Listener for FREBottomGroup.
     */
    public interface Listener {
        /**
         * Notifies when the user clicked the "add account" button.
         */
        void addAccount();

        /**
         * Notifies when the user clicked the dismiss button.
         */
        void advanceToNextPage();
    }

    private final FREBottomGroupMediator mMediator;

    /**
     * Constructs a coordinator instance.
     */
    @MainThread
    public FREBottomGroupCoordinator(
            Context context, View view, ModalDialogManager modalDialogManager, Listener listener) {
        mMediator = new FREBottomGroupMediator(context, modalDialogManager, listener);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), view, FREBottomGroupViewBinder::bind);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    @MainThread
    public void destroy() {
        mMediator.destroy();
    }
}
