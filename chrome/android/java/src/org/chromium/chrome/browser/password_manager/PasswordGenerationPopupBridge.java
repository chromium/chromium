// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.os.Handler;
import android.view.View;
import android.widget.PopupWindow;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;

/** JNI call glue for password generation between native and Java objects. */
public class PasswordGenerationPopupBridge implements PopupWindow.OnDismissListener {
    private final long mNativePasswordGenerationEditingPopupViewAndroid;
    private final Context mContext;
    private final DropdownPopupWindow mPopup;
    private final View mAnchorView;

    /**
     * A convenience method for the constructor to be invoked from the native counterpart.
     * @param anchorView View anchored for popup.
     * @param nativePopup The pointer to the native counterpart.
     * @param windowAndroid The browser window.
     */
    @CalledByNative
    private static PasswordGenerationPopupBridge create(
            View anchorView, long nativePopup, WindowAndroid windowAndroid) {
        return new PasswordGenerationPopupBridge(anchorView, nativePopup, windowAndroid);
    }

    /**
     * Builds the bridge between native and Java objects.
     * @param anchorView View anchored for popup.
     * @param nativePopup The pointer to the native counterpart.
     * @param windowAndroid The browser window.
     */
    public PasswordGenerationPopupBridge(
            View anchorView, long nativePopup, WindowAndroid windowAndroid) {
        mNativePasswordGenerationEditingPopupViewAndroid = nativePopup;
        mContext = windowAndroid.getActivity().get();
        mAnchorView = anchorView;
        // mContext could've been garbage collected.
        if (mContext == null) {
            mPopup = null;
            // Prevent destroying the native counterpart when it's about to derefence its own
            // members in UpdateBoundsAndRedrawPopup().
            new Handler().post(this::onDismiss);
        } else {
            mPopup = new DropdownPopupWindow(mContext, anchorView);
            mPopup.setOnDismissListener(this);
            mPopup.disableHideOnOutsideTap();
            mPopup.setContentDescriptionForAccessibility(
                    mContext.getString(R.string.password_generation_popup_content_description));
        }
    }

    /**
     * Handles dismissing the popup window. The native counterpart is notified to destroy the
     * controller.
     */
    @Override
    public void onDismiss() {
        PasswordGenerationPopupBridgeJni.get()
                .dismissed(
                        mNativePasswordGenerationEditingPopupViewAndroid,
                        PasswordGenerationPopupBridge.this);
    }

    /**
     * Shows a password generation popup with specified data. Should be called after
     * setAnchorRect().
     *
     * @param isRtl True if the popup should be RTL.
     * @param explanationText The translated text that explains the popup.
     */
    @CalledByNative
    private void show(boolean isRtl, @JniType("std::u16string") String explanationText) {
        if (mPopup != null) {
            float anchorWidth = mAnchorView.getLayoutParams().width;
            assert anchorWidth > 0;
            PasswordGenerationPopupAdapter adapter =
                    new PasswordGenerationPopupAdapter(mContext, explanationText, anchorWidth);
            mPopup.setAdapter(adapter);
            mPopup.setRtl(isRtl);
            mPopup.show();
        }
    }

    /** Hides the password generation popup. */
    @CalledByNative
    private void hide() {
        if (mPopup != null) mPopup.dismiss();
    }

    @NativeMethods
    interface Natives {
        void dismissed(
                long nativePasswordGenerationEditingPopupViewAndroid,
                PasswordGenerationPopupBridge caller);
    }
}
