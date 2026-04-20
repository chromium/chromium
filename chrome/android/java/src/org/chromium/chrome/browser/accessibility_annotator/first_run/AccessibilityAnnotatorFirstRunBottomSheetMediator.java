// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Mediator for the Accessibility Annotator first-run bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorFirstRunBottomSheetMediator {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final AccessibilityAnnotatorFirstRunBottomSheetContent mContent;
    private final AccessibilityAnnotatorFirstRunBottomSheetCoordinator.Delegate mDelegate;
    private final SettingsCustomTabLauncher mCustomTabLauncher;

    private @Nullable String mManageSettingsUrl;
    private @Nullable String mLearnMoreUrl;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                        mDelegate.onInfoDismissed();
                    }
                    mBottomSheetController.removeObserver(this);
                }
            };

    AccessibilityAnnotatorFirstRunBottomSheetMediator(
            Context context,
            BottomSheetController bottomSheetController,
            AccessibilityAnnotatorFirstRunBottomSheetContent content,
            AccessibilityAnnotatorFirstRunBottomSheetCoordinator.Delegate delegate,
            SettingsCustomTabLauncher customTabLauncher) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContent = content;
        mDelegate = delegate;
        mCustomTabLauncher = customTabLauncher;
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param manageSettingsUrl The URL for the manage settings page.
     * @param learnMoreUrl The URL for the learn more page.
     * @return True if the content was shown, false if it was suppressed.
     */
    boolean requestShowContent(String manageSettingsUrl, String learnMoreUrl) {
        mManageSettingsUrl = manageSettingsUrl;
        mLearnMoreUrl = learnMoreUrl;
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            return false;
        }
        return true;
    }

    /** Handles the acknowledge action. */
    void onAcknowledgeClicked() {
        mDelegate.onInfoAcknowledged();
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Handles the manage settings action. */
    void onManageSettingsClicked() {
        if (mManageSettingsUrl != null) {
            mCustomTabLauncher.openUrlInCct(mContext, mManageSettingsUrl);
        }
        mDelegate.onManageSettingsClicked();
    }

    /** Handles the learn more link click. */
    void onLearnMoreClicked() {
        if (mLearnMoreUrl != null) {
            mCustomTabLauncher.openUrlInCct(mContext, mLearnMoreUrl);
        }
        mDelegate.onLearnMoreClicked();
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mBottomSheetController.hideContent(mContent, /* animate= */ true, hideReason);
    }
}
