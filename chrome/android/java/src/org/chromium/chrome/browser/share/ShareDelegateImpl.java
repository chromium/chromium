// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Implementation of share interface. Mostly a wrapper around ShareSheetCoordinator.
 */
public class ShareDelegateImpl implements ShareDelegate {
    private Callback<BottomSheetController> mBottomSheetControllerSupplierCallback;
    private BottomSheetController mBottomSheetController;

    /**
     * Construct a new {@link ShareDelegateImpl}.
     * @param controllerSupplier The ObservableSupplier that will notify this class when the
     *                           BottomSheetController has been initialized.
     */
    public ShareDelegateImpl(ObservableSupplier<BottomSheetController> controllerSupplier) {
        mBottomSheetControllerSupplierCallback = bottomSheetController -> {
            mBottomSheetController = bottomSheetController;
        };
        controllerSupplier.addObserver(mBottomSheetControllerSupplierCallback);
    }

    // ShareDelegate implementation.
    @Override
    public void share(ShareParams params) {
        createCoordinator().share(params);
    }

    // ShareDelegate implementation.
    @Override
    public void share(Tab currentTab, boolean shareDirectly) {
        createCoordinator().onShareSelected(currentTab.getWindowAndroid().getActivity().get(),
                currentTab, shareDirectly, currentTab.isIncognito());
    }

    private ShareSheetCoordinator createCoordinator() {
        ShareSheetCoordinator coordinator = new ShareSheetCoordinator(mBottomSheetController);
        return coordinator;
    }
}
