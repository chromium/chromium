// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Builds and shows a basic ConfirmInfoBar for code that works almost entirely in Java.
 *
 * Rather than use this class, it is highly recommended that developers create and customize their
 * own customized native InfoBarDelegate to avoid unnecessary JNI hops.
 */
public class SimpleConfirmInfoBarBuilder {

    /**
     * Listens for when users interact with an infobar.
     */
    public static interface Listener {

        /**
         * Called when the infobar was dismissed.
         */
        void onInfoBarDismissed();

        /**
         * Called when either the primary or secondary button of a ConfirmInfoBar is clicked.
         *
         * @param isPrimary True if the primary button was clicked, false if the secondary button
         *                  was clicked.
         * @return True if the listener caused the closing of this info bar as a side effect,
         *         false otherwise.
         */
        boolean onInfoBarButtonClicked(boolean isPrimary);

        /**
         * Called when the link of a ConfirmInfoBar is clicked.
         * @return True if the listener caused the closing of this info bar as a side effect,
         *         false otherwise.
         */
        boolean onInfoBarLinkClicked();
    }

    /**
     * Creates a simple infobar to display a message to the user.
     *
     * Consider using snackbars instead of this function for ephemeral messages.
     *
     * @param tab Tab to attach the infobar to.
     * @param infobarTypeIdentifier Unique ID defined in the C++ InfoBarDelegate::InfoBarIdentifier.
     * @param message Message displayed to the user.
     * @param autoExpire Whether the infobar disappears on navigation.
     */
    public static void create(
            Tab tab, int infobarTypeIdentifier, String message, boolean autoExpire) {
        create(tab, null, infobarTypeIdentifier, 0, message, null, null, null, autoExpire);
    }

    /**
     * Creates a simple infobar to prompt the user.
     *
     * @param tab Tab to attach the infobar to.
     * @param listener Alerted when the user interacts with the infobar.
     * @param infobarTypeIdentifier Unique ID defined in the C++ InfoBarDelegate::InfoBarIdentifier.
     * @param drawableId Resource ID of the icon representing the infobar.
     * @param message Message displayed to the user.
     * @param primaryText String shown on the primary ConfirmInfoBar button.
     * @param secondaryText String shown on the secondary ConfirmInfoBar button.
     * @param linkText String shown as the link text on the ConfirmInfoBar.
     * @param autoExpire Whether the infobar disappears on navigation.
     */
    public static void create(Tab tab, Listener listener, int infobarTypeIdentifier, int drawableId,
            String message, String primaryText, String secondaryText, String linkText,
            boolean autoExpire) {
        Activity activity = tab.getWindowAndroid().getActivity().get();
        Bitmap drawable = activity == null || drawableId == 0 ? null
                : BitmapFactory.decodeResource(activity.getResources(), drawableId);
        SimpleConfirmInfoBarBuilderJni.get().create(tab, infobarTypeIdentifier, drawable, message,
                primaryText, secondaryText, linkText, autoExpire, listener);
    }

    @CalledByNative
    private static void onInfoBarDismissed(Listener listener) {
        if (listener != null) listener.onInfoBarDismissed();
    }

    @CalledByNative
    private static boolean onInfoBarButtonClicked(Listener listener, boolean isPrimary) {
        return listener == null ? false : listener.onInfoBarButtonClicked(isPrimary);
    }

    @CalledByNative
    private static boolean onInfoBarLinkClicked(Listener listener) {
        return listener == null ? false : listener.onInfoBarLinkClicked();
    }

    @NativeMethods
    interface Natives {
        void create(Tab tab, int infobarTypeIdentifier, Bitmap drawable, String message,
                String primaryText, String secondaryText, String linkText, boolean autoExpire,
                Object listener);
    }
}

