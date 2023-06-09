// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.util.TokenHolder;

/**
 * Handles interaction with other UI's when a bottom sheet goes in and out of expanded mode:
 * <ul>
 * <li> Suspends modal dialogs
 * <li> Hides the tab from accessibility tree
 * </ul>
 */
public class ExpandedSheetHelperImpl implements ExpandedSheetHelper {
    /** A token for suppressing app modal dialogs. */
    private int mAppModalToken = TokenHolder.INVALID_TOKEN;

    /** A token for suppressing tab modal dialogs. */
    private int mTabModalToken = TokenHolder.INVALID_TOKEN;

    /** A delegate that provides the functionality of obscuring all tabs. */
    private TabObscuringHandler mTabObscuringHandler;

    /** A token held while the bottom sheet is obscuring all visible tabs. */
    private TabObscuringHandler.Token mTabObscuringToken;

    /** A supplier of the activity's dialog manager. */
    private Supplier<ModalDialogManager> mDialogManager;

    public ExpandedSheetHelperImpl(
            Supplier<ModalDialogManager> dialogManager, TabObscuringHandler tabObscuringHandler) {
        mDialogManager = dialogManager;
        mTabObscuringHandler = tabObscuringHandler;
    }

    /**
     * Called when sheet is opened. Suspends modal dialogs, and hides the tab from
     * the accessibility tree.
     */
    @Override
    public void onSheetExpanded() {
        setIsObscuringAllTabs(true);

        assert mAppModalToken == TokenHolder.INVALID_TOKEN;
        assert mTabModalToken == TokenHolder.INVALID_TOKEN;
        if (mDialogManager.get() != null) {
            mAppModalToken = mDialogManager.get().suspendType(ModalDialogType.APP);
            mTabModalToken = mDialogManager.get().suspendType(ModalDialogType.TAB);
        }
    }

    /**
     * Called when sheet is closed. Resumes the suspended modal dialog if any, and
     * the tab's accessibility tree.
     */
    @Override
    public void onSheetCollapsed() {
        setIsObscuringAllTabs(false);

        // Tokens can be invalid if the sheet has a custom lifecycle.
        if (mDialogManager.get() != null
                && (mAppModalToken != TokenHolder.INVALID_TOKEN
                        || mTabModalToken != TokenHolder.INVALID_TOKEN)) {
            // If one modal dialog token is set, the other should be as well.
            assert mAppModalToken != TokenHolder.INVALID_TOKEN
                    && mTabModalToken != TokenHolder.INVALID_TOKEN;
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.APP, mAppModalToken);
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.TAB, mTabModalToken);
        }
        mAppModalToken = TokenHolder.INVALID_TOKEN;
        mTabModalToken = TokenHolder.INVALID_TOKEN;
    }

    /**
     * Set whether the bottom sheet is obscuring all tabs.
     * @param isObscuring Whether the bottom sheet is considered to be obscuring.
     */
    private void setIsObscuringAllTabs(boolean isObscuring) {
        if (isObscuring) {
            assert mTabObscuringToken == null;
            mTabObscuringToken =
                    mTabObscuringHandler.obscure(TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR);
        } else {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
    }
}
