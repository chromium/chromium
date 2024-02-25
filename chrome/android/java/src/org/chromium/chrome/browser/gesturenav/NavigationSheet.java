// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationHistory;

/** Interface that defines the methods for controlling Navigation sheet. */
public interface NavigationSheet {
    /** Delegate performing navigation-related operations/providing the required info. */
    interface Delegate {
        /**
         * @param forward {@code true} if the requested history is of forward navigation.
         * @param isOffTheRecord {@code true} if the history is called from incognito mode.
         * @return {@link NavigationHistory} object.
         */
        NavigationHistory getHistory(boolean forward, boolean isOffTheRecord);

        /** Navigates to the page associated with the given index. */
        void navigateToIndex(int index);
    }

    /**
     * Create {@link NavigationSheet} object.
     * @param rootView Root view whose dimension is used for the sheet.
     * @param context {@link Context} used to retrieve resources.
     * @param bottomSheetController {@link BottomSheetController} object.
     * @return NavigationSheet object.
     */
    public static NavigationSheet create(
            View rootView,
            Context context,
            Supplier<BottomSheetController> bottomSheetController,
            Profile profile) {
        return new NavigationSheetCoordinator(rootView, context, bottomSheetController, profile);
    }

    /**
     * @return {@code true} if navigation sheet is enabled.
     */
    static boolean isEnabled() {
        return false;
    }

    /**
     * @return {@code true} if another instance of NavigationSheet is already showing.
     */
    public static boolean isInstanceShowing(BottomSheetController controller) {
        if (controller == null) return false;
        return (controller.getCurrentSheetContent() instanceof NavigationSheetCoordinator)
                && controller.isSheetOpen();
    }

    /** Placeholder object that does nothing. Saves lots of null checks. */
    static final NavigationSheet PLACEHOLDER =
            new NavigationSheet() {
                @Override
                public void setDelegate(Delegate delegate) {}

                @Override
                public void start(boolean forward, boolean showCloseIndicator) {}

                @Override
                public boolean startAndExpand(boolean forward, boolean animate) {
                    return false;
                }

                @Override
                public void close(boolean animate) {}

                @Override
                public void onScroll(float delta, float overscroll, boolean willNavigate) {}

                @Override
                public void release() {}

                @Override
                public boolean isHidden() {
                    return true;
                }

                @Override
                public boolean isExpanded() {
                    return false;
                }
            };

    /**
     * Set a new {@link Delegate} object whenever the dependency is updated.
     * @param delegate Delegate used by navigation sheet to perform actions.
     */
    void setDelegate(Delegate delegate);

    /**
     * Get the navigation sheet ready as the gesture starts.
     * @param forward {@code true} if we're navigating forward.
     * @param showCloseIndicator {@code true} if we should show 'close chrome' indicator
     *        on the arrow puck.
     */
    void start(boolean forward, boolean showCloseIndicator);

    /**
     * Fully expand the navigation sheet from the beginning.
     * @param forward {@code true} if this is for forward navigation.
     * @param animate {@code true} to enable animation.
     * @return {@code true} if the sheet is opened as expected.
     */
    boolean startAndExpand(boolean forward, boolean animate);

    /**
     * Close the navigation sheet.
     * @param animate {@code true} to enable animation.
     */
    void close(boolean animate);

    /**
     * Process swipe gesture and update the navigation sheet state.
     * @param delta Scroll delta from the previous scroll.
     * @param overscroll Total amount of scroll since the dragging started.
     * @param willNavigate {@code true} if navgation will be triggered upon release.
     */
    void onScroll(float delta, float overscroll, boolean willNavigate);

    /** Process release events. */
    void release();

    /**
     * @param {@code true} if navigation sheet is in hidden state.
     */
    boolean isHidden();

    /**
     * @param {@code true} if navigation sheet is in expanded (half/full) state.
     */
    boolean isExpanded();
}
