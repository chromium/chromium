// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.v4.view.ViewCompat;
import android.view.View;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarContainerObserver;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * A helper class to managing showing and dismissing in-product help dialogs based on which infobar
 * is frontmost and showing.  This will show an in-product help window when a new relevant infobar
 * becomes front-most.  If that infobar is closed or another infobar comes to the front the window
 * will be dismissed.
 */
public class IPHInfoBarSupport implements OnDismissListener,
                                          InfoBarContainer.InfoBarAnimationListener,
                                          InfoBarContainerObserver {
    /** Helper class to hold all relevant display parameters for an in-product help window. */
    public static class TrackerParameters {
        public TrackerParameters(
                String feature, @StringRes int textId, @StringRes int accessibilityTextId) {
            this.feature = feature;
            this.textId = textId;
            this.accessibilityTextId = accessibilityTextId;
        }

        /** @see FeatureConstants */
        public String feature;

        @StringRes
        public int textId;

        @StringRes
        public int accessibilityTextId;
    }

    /** Helper class to manage state relating to a particular instance of an in-product window. */
    public static class PopupState {
        /** The View that represents the infobar that the in-product window is attached to. */
        public View view;

        /** The bubble that is currently showing the in-product help. */
        public TextBubble bubble;

        /** The in-product help feature that the popup relates to. */
        public String feature;
    }

    /**
     * Delegate responsible for interacting with the in-product help backend and creating any
     * {@link TextBubble}s if necessary.
     */
    public static interface IPHBubbleDelegate {
        /**
         * Will be called when a valid infobar of type {@code infoBarId} is showing and is attached
         * to the view hierarchy.
         * @param anchorView The {@link View} the {@link TextBubble} should be attached to.
         * @param infoBarId  The id representing the type of infobar to potentially show an
         *                   in-product help for.
         * @return           {@code null} if no bubble should be shown.  Otherwise a valid
         *                   {@link PopupState} representing the current state of the shown
         *                   {@link TextBubble}.
         */
        PopupState createStateForInfoBar(View anchorView, @InfoBarIdentifier int infoBarId);

        /**
         * Will be called when the {@link TextBubble} related to the currently showing infobar has
         * been dismissed.
         * @param state The {@link PopupState} that represents the {@link TextBubble} and state
         *              created from an earlier call to {@link #createStateForInfoBar(View, int)}.
         */
        void onPopupDismissed(PopupState state);
    }

    /**
     * The delegate responsible for interacting with external components (Creating a TextBubble and
     * interacting with the IPH backend.
     */
    private final IPHBubbleDelegate mDelegate;

    /** The state of the currently showing in-product window or {@code null} if none is showing. */
    private PopupState mCurrentState;

    /** Creates a new instance of an IPHInfoBarSupport class. */
    IPHInfoBarSupport(IPHBubbleDelegate delegate) {
        mDelegate = delegate;
    }

    // InfoBarContainer.InfoBarAnimationListener implementation.
    @Override
    public void notifyAnimationFinished(int animationType) {}

    // Calling {@link TextBubble#dismiss()} will invoke {@link #onDismiss} which will
    // set the value of {@link #mCurrentState} to null, which is what the assert checks. Since this
    // goes through the Android SDK, FindBugs does not see this as happening, so the FindBugs
    // warning for a field guaranteed to be non-null being checked for null equality needs to be
    // suppressed.
    @Override
    public void notifyAllAnimationsFinished(Item frontInfoBar) {
        View view = frontInfoBar == null ? null : frontInfoBar.getView();

        if (mCurrentState != null) {
            // Clean up any old infobar if necessary.
            if (mCurrentState.view != view) {
                mCurrentState.bubble.dismiss();
                assert mCurrentState == null;
            }
        }

        if (frontInfoBar == null || view == null || !ViewCompat.isAttachedToWindow(view)) return;

        mCurrentState = mDelegate.createStateForInfoBar(view, frontInfoBar.getInfoBarIdentifier());
        if (mCurrentState == null) return;

        mCurrentState.bubble.addOnDismissListener(this);
        mCurrentState.bubble.show();
    }

    // InfoBarContainerObserver implementation.
    @Override
    public void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst) {}

    // Calling {@link TextBubble#dismiss()} will invoke {@link #onDismiss} which will
    // set the value of {@link #mCurrentState} to null, which is what the assert checks. Since this
    // goes through the Android SDK, FindBugs does not see this as happening, so the FindBugs
    // warning for a field guaranteed to be non-null being checked for null equality needs to be
    // suppressed.
    @Override
    public void onRemoveInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isLast) {
        if (mCurrentState != null && infoBar.getView() == mCurrentState.view) {
            mCurrentState.bubble.dismiss();
            assert mCurrentState == null;
        }
    }

    @Override
    public void onInfoBarContainerAttachedToWindow(boolean hasInfobars) {}

    @Override
    public void onInfoBarContainerShownRatioChanged(InfoBarContainer container, float shownRatio) {}

    // PopupWindow.OnDismissListener implementation.
    @Override
    public void onDismiss() {
        if (mCurrentState == null) return;
        mDelegate.onPopupDismissed(mCurrentState);
        mCurrentState = null;
    }


}
