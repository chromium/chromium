// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;

/**
 * Represents an item (either selectable or not) presented on the UI, such as the username
 * or a credit card number.
 */
public final class UserInfoField {
    private final String mDisplayText;
    private final String mA11yDescription;
    private final String mId;
    private final boolean mIsObfuscated;
    private final Callback<UserInfoField> mCallback;

    /**
     * @param displayText The text to display. Plain text if |isObfuscated| is false.
     * @param a11yDescription The description used for accessibility.
     * @param id An ID representing this object for filling purposes. May be empty.
     * @param isObfuscated If true, the displayed caption is transformed into stars.
     * @param callback Called when the user taps the suggestions.
     */
    public UserInfoField(String displayText, String a11yDescription, String id,
            boolean isObfuscated, Callback<UserInfoField> callback) {
        mDisplayText = displayText;
        mA11yDescription = a11yDescription;
        mId = id;
        mIsObfuscated = isObfuscated;
        mCallback = callback;
    }

    /**
     * Returns the text to be displayed on the UI.
     */
    @CalledByNative
    public String getDisplayText() {
        return mDisplayText;
    }

    /**
     * Returns a translated description that can be used for accessibility.
     */
    @CalledByNative
    public String getA11yDescription() {
        return mA11yDescription;
    }

    /**
     * Returns an ID representing this object for filling purposes. May be empty.
     */
    @CalledByNative
    public String getId() {
        return mId;
    }

    /**
     * Returns whether the user can interact with the selected suggestion. For example,
     * this is false if this is a password suggestion on a non-password input field.
     */
    @CalledByNative
    public boolean isSelectable() {
        return mCallback != null;
    }

    /**
     * Returns true if obfuscation should be applied to the item's caption, for example to
     * hide passwords.
     */
    @CalledByNative
    public boolean isObfuscated() {
        return mIsObfuscated;
    }

    /**
     * The delegate is called when the Item is selected by a user.
     */
    public void triggerSelection() {
        if (mCallback != null) mCallback.onResult(this);
    }
}
