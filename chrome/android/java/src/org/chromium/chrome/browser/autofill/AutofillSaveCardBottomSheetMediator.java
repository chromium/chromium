// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

import java.util.function.Consumer;

/**
 * Mediator class for the autofill save card UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a payment card (either
 * locally or uploaded).
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiShown, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class AutofillSaveCardBottomSheetMediator extends EmptyBottomSheetObserver
        implements AutofillSaveCardBottomSheetContent.Delegate,
                TabModelObserver,
                LayoutStateObserver {
    private final AutofillSaveCardBottomSheetContent mBottomSheetContent;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;
    private final Consumer<String> mOnUiLegalMessageUrlClicked;
    private final AutofillSaveCardBottomSheetBridge mBridge;
    private boolean mFinished;

    /**
     * Creates the mediator.
     *
     * @param bottomSheetContent The bottom sheet content to be shown.
     * @param uiInfo The assets (icons and text) displayed in the bottom sheet.
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     * to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after
     * a tab change.
     * @param onUiLegalMessageUrlClicked Called when a legal message url was clicked.
     * @param bridge The bridge to signal UI flow events (OnUiShown, OnUiAccepted, etc.) to.
     */
    AutofillSaveCardBottomSheetMediator(
            AutofillSaveCardBottomSheetContent bottomSheetContent,
            AutofillSaveCardUiInfo uiInfo,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            Consumer<String> onUiLegalMessageUrlClicked,
            AutofillSaveCardBottomSheetBridge bridge) {
        mBottomSheetContent = bottomSheetContent;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;
        mOnUiLegalMessageUrlClicked = onUiLegalMessageUrlClicked;
        mBridge = bridge;

        mBottomSheetContent.setDelegate(this);
        mBottomSheetContent.setUiInfo(uiInfo);
        mBottomSheetController.addObserver(this);
        mLayoutStateProvider.addObserver(this);
        mTabModel.addObserver(this);
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            mBridge.onUiShown();
        } else {
            mBridge.onUiIgnored();
        }
    }

    /** Hide the bottom sheet (if showing) and clean up observers. */
    void destroy() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
        mBottomSheetController.removeObserver(this);
        mLayoutStateProvider.removeObserver(this);
        mTabModel.removeObserver(this);
        finish(AutofillSaveCardBottomSheetBridge::onUiIgnored);
    }

    private void finish(Consumer<AutofillSaveCardBottomSheetBridge> bridgeCallback) {
        if (!mFinished) {
            mFinished = true;
            bridgeCallback.accept(mBridge);
        }
    }

    // AutofillSaveCardBottomSheetContent.Delegate implementation follows:
    @Override
    public void didClickLegalMessageUrl(String url) {
        mOnUiLegalMessageUrlClicked.accept(url);
    }

    @Override
    public void didClickConfirm() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        finish(AutofillSaveCardBottomSheetBridge::onUiAccepted);
    }

    @Override
    public void didClickCancel() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        finish(AutofillSaveCardBottomSheetBridge::onUiCanceled);
    }

    // EmptyBottomSheetObserver overridden methods follow:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS: // Intentionally fall through.
            case StateChangeReason.SWIPE: // Intentionally fall through.
            case StateChangeReason.TAP_SCRIM:
                finish(AutofillSaveCardBottomSheetBridge::onUiCanceled);
                break;
            case StateChangeReason.INTERACTION_COMPLETE:
                // Expecting didClickConfirm() and didClickCancel() call the delegate in this case.
                break;
            default:
                finish(AutofillSaveCardBottomSheetBridge::onUiIgnored);
                break;
        }
    }

    // TabModelObserver
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        // While the bottom sheet scrim covers the omnibox UI, a new tab can be created in other
        // ways such as by opening a link from another app. In this case we want to hide the bottom
        // sheet rather than keeping the bottom sheet open while this tab loads behind the scrim.
        if (lastId != tab.getId()) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
            finish(AutofillSaveCardBottomSheetBridge::onUiIgnored);
        }
    }

    // LayoutStateObserver
    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        // When the browser layout changes away from browsing to say the tab switcher, then the
        // bottom sheet must be hidden.
        if (layoutType != LayoutType.BROWSING) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
            finish(AutofillSaveCardBottomSheetBridge::onUiIgnored);
        }
    }
}
