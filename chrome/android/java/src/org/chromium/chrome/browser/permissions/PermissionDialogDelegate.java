// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Delegate class for modal permission dialogs. Contains all of the data displayed in a prompt,
 * including the button strings, message text and the icon.
 *
 * This class is also the interface to the native-side permissions code. When the user responds to
 * the permission dialog, the decision is conveyed across the JNI so that the native code can
 * respond appropriately.
 */
public class PermissionDialogDelegate {
    /** The native-side counterpart of this class */
    private long mNativeDelegatePtr;

    /** The controller for this class */
    private PermissionDialogController mDialogController;

    /** The tab for which to create the dialog. */
    private Tab mTab;

    /** The icon to display in the dialog. */
    private int mDrawableId;

    /** Title text that can be shown in the dialog. */
    private String mTitleText;

    /** Text shown in the dialog. */
    private String mMessageText;

    /** Text shown on the primary button, e.g. "Allow". */
    private String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Block". */
    private String mSecondaryButtonText;

    /** The {@link ContentSettingsType}s requested in this dialog.  */
    private int[] mContentSettingsTypes;

    public Tab getTab() {
        return mTab;
    }

    public int[] getContentSettingsTypes() {
        return mContentSettingsTypes.clone();
    }

    public int getDrawableId() {
        return mDrawableId;
    }

    public String getTitleText() {
        return mTitleText;
    }

    public String getMessageText() {
        return mMessageText;
    }

    public String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    public String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    public void onAccept() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().accept(mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void onCancel() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().cancel(mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void onDismiss() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().dismissed(
                mNativeDelegatePtr, PermissionDialogDelegate.this);
    }

    public void destroy() {
        assert mNativeDelegatePtr != 0;
        PermissionDialogDelegateJni.get().destroy(
                mNativeDelegatePtr, PermissionDialogDelegate.this);
        mNativeDelegatePtr = 0;
    }

    public void setDialogController(PermissionDialogController controller) {
        mDialogController = controller;
    }

    /**
     * Called from C++ by |nativeDelegatePtr| to destroy the dialog.
     */
    @CalledByNative
    private void dismissFromNative() {
        mDialogController.dismissFromNative(this);
    }

    /**
     * Called from C++ by |nativeDelegatePtr| to instantiate this class.
     *
     * @param nativeDelegatePtr     The native counterpart that this object owns.
     * @param tab                   The tab to create the dialog for.
     * @param contentSettingsTypes  The content settings types requested by this dialog.
     * @param iconResourceId        The id of the icon to display in the dialog.
     * @param title                 The title to display in the dialog. This is the permission type.
     * @param message               The message to display in the dialog.
     * @param primaryTextButton     The text to display on the primary button.
     * @param secondaryTextButton   The text to display on the primary button.
     */
    @CalledByNative
    private static PermissionDialogDelegate create(long nativeDelegatePtr, Tab tab,
            int[] contentSettingsTypes, int enumeratedIconId, String title, String message,
            String primaryButtonText, String secondaryButtonText) {
        return new PermissionDialogDelegate(nativeDelegatePtr, tab, contentSettingsTypes,
                enumeratedIconId, title, message, primaryButtonText, secondaryButtonText);
    }

    /**
     * Upon construction, this class takes ownership of the passed in native delegate.
     */
    private PermissionDialogDelegate(long nativeDelegatePtr, Tab tab, int[] contentSettingsTypes,
            int enumeratedIconId, String title, String message, String primaryButtonText,
            String secondaryButtonText) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mTab = tab;
        mContentSettingsTypes = contentSettingsTypes;
        mDrawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        mTitleText = title;
        mMessageText = message;
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
    }

    @NativeMethods
    interface Natives {
        void accept(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);
        void cancel(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);
        void dismissed(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);
        void destroy(long nativePermissionDialogDelegate, PermissionDialogDelegate caller);
    }
}
