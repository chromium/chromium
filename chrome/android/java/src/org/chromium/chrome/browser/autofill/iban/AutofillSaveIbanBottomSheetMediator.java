// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * Mediator class for the autofill IBAN save UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a IBAN.
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiCanceled, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class AutofillSaveIbanBottomSheetMediator extends EmptyBottomSheetObserver
        implements TabModelObserver, LayoutStateObserver {
    private final AutofillSaveIbanBottomSheetCoordinator.NativeDelegate mDelegate;
    private final AutofillSaveIbanBottomSheetContent mBottomSheetContent;
    private boolean mFinished;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModel mTabModel;

    /**
     * Creates the mediator.
     *
     * @param delegate The delegate to signal UI flow events (OnUiCanceled, OnUiAccepted, etc.).
     * @param bottomSheetContent The bottom sheet content to be shown.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     */
    AutofillSaveIbanBottomSheetMediator(
            AutofillSaveIbanBottomSheetCoordinator.NativeDelegate delegate,
            AutofillSaveIbanBottomSheetContent bottomSheetContent,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel) {
        mDelegate = delegate;
        mBottomSheetContent = bottomSheetContent;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;

        mBottomSheetController.addObserver(this);
        mLayoutStateProvider.addObserver(this);
        mTabModel.addObserver(this);
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            // TODO(b/309163770): call delegate functions.
        } else {
            mDelegate.onUiIgnored();
        }
    }

    public void onAccepted(String userProvidedNickname) {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiAccepted(userProvidedNickname);
    }

    public void onCanceled() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiCanceled();
    }

    void hide(@StateChangeReason int hideReason) {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true, hideReason);
        mBottomSheetController.removeObserver(this);
        mLayoutStateProvider.removeObserver(this);
        mTabModel.removeObserver(this);
    }

    // TabModelObserver.
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        // While the bottom sheet scrim covers the omnibox UI, a new tab can be created in other
        // ways such as by opening a link from another app. In this case we want to hide the bottom
        // sheet rather than keeping the bottom sheet open while this tab loads behind the scrim.
        if (lastId != tab.getId()) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
        }
    }

    // LayoutStateObserver.
    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        // When the browser layout changes away from browsing to say the tab switcher, then the
        // bottom sheet must be hidden.
        if (layoutType != LayoutType.BROWSING) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
        }
    }
}
