// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * The lifecycle for the save card bottom sheet. Notifies the caller when a tab or layout changes
 * (e.g., going into the "tab overview"), so the bottom sheet can be dismissed. Ignores page
 * navigation.
 */
/*package*/ class AutofillSaveCardBottomSheetLifecycle extends EmptyBottomSheetObserver
        implements TabModelObserver, LayoutStateProvider.LayoutStateObserver {
    /** Controller callbacks from the save card bottom sheet. */
    interface ControllerDelegate {
        void onCanceled();

        void onIgnored();
    }

    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    private ControllerDelegate mDelegate;
    private boolean mFinished;

    /**
     * Constructs the lifecycle for the save card bottom sheet.
     *
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     */
    AutofillSaveCardBottomSheetLifecycle(
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel) {
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;
    }

    /**
     * Begins the lifecycle of the save card bottom sheet. Starts observing tab and layout changes.
     *
     * @param delegate The controller callbacks for user actions.
     */
    void begin(ControllerDelegate delegate) {
        mDelegate = delegate;

        mBottomSheetController.addObserver(this);
        mLayoutStateProvider.addObserver(this);
        mTabModel.addObserver(this);
    }

    /** Ends the lifecycle of the save card bottom sheet. Stops observing tab and layout changes. */
    void end() {
        mTabModel.removeObserver(this);
        mLayoutStateProvider.removeObserver(this);
        mBottomSheetController.removeObserver(this);
    }

    // Overrides EmptyBottomSheetObserver onSheetClosed method for BottomSheetController.
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.SWIPE:
            case StateChangeReason.TAP_SCRIM:
                finish(mDelegate::onCanceled);
                break;
            case StateChangeReason.INTERACTION_COMPLETE:
                // Expecting AutofillSaveCardBottomSheetCoordinator to set up the appropriate
                // callbacks to native on button presses in this case.
                mFinished = true;
                break;
            default:
                finish(mDelegate::onIgnored);
                break;
        }
    }

    // Implements LayoutStateObserver for LayoutStateProvider.
    @Override
    public void onStartedShowing(int layoutType) {
        if (layoutType != LayoutType.BROWSING) {
            finish(mDelegate::onIgnored);
        }
    }

    // Implements TabModelObserver for TabModel.
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        if (lastId != tab.getId()) {
            finish(mDelegate::onIgnored);
        }
    }

    void finish(Runnable callback) {
        if (!mFinished) {
            mFinished = true;
            callback.run();
        }
    }
}
