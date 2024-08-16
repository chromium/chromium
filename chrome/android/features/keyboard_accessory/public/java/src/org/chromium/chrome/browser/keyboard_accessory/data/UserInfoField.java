// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;

/**
 * Represents an item (either selectable or not) presented on the UI, such as the username
 * or a credit card number.
 */
public final class UserInfoField {
    private final String mDisplayText;
    private final String mTextToFill;
    private final String mA11yDescription;
    private final String mId;
    private final int mIconId;
    private final boolean mIsObfuscated;
    private final Callback<UserInfoField> mCallback;

    /**
     * @param displayText The text to display. Plain text if |isObfuscated| is false.
     * @param a11yDescription The description used for accessibility.
     * @param id An ID representing this object for filling purposes. May be empty.
     * @param isObfuscated If true, the displayed caption is transformed into stars.
     * @param callback Called when the user taps the suggestions.
     */
    @Deprecated
    public UserInfoField(
            String displayText,
            String a11yDescription,
            String id,
            boolean isObfuscated,
            Callback<UserInfoField> callback) {
        this(
                displayText,
                displayText,
                a11yDescription,
                id,
                /* iconId= */ 0,
                isObfuscated,
                callback);
    }

    /**
     * @param displayText The text to display. Plain text if |isObfuscated| is false.
     * @param textToFill The text that would be filled in the form field when clicked.
     * @param a11yDescription The description used for accessibility.
     * @param id An ID representing this object for filling purposes. May be empty.
     * @param isObfuscated If true, the displayed caption is transformed into stars.
     * @param callback Called when the user taps the suggestions.
     */
    private UserInfoField(
            String displayText,
            String textToFill,
            String a11yDescription,
            String id,
            int iconId,
            boolean isObfuscated,
            Callback<UserInfoField> callback) {
        mDisplayText = displayText;
        mTextToFill = textToFill;
        mA11yDescription = a11yDescription;
        mId = id;
        mIconId = iconId;
        mIsObfuscated = isObfuscated;
        mCallback = callback;
    }

    /** Returns the text to be displayed on the UI. */
    @CalledByNative
    public String getDisplayText() {
        return mDisplayText;
    }

    /** Returns the text to be filled in the form field. */
    @CalledByNative
    public String getTextToFill() {
        return mTextToFill;
    }

    /** Returns a translated description that can be used for accessibility. */
    @CalledByNative
    public String getA11yDescription() {
        return mA11yDescription;
    }

    /** Returns an ID representing this object for filling purposes. May be empty. */
    @CalledByNative
    public String getId() {
        return mId;
    }

    /** Returns an icon id for the user info field. Might be equal to 0. */
    public int getIconId() {
        return mIconId;
    }

    /**
     * Returns whether the user can interact with the selected suggestion. For example, this is
     * false if this is a password suggestion on a non-password input field.
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

    /** The delegate is called when the Item is selected by a user. */
    public void triggerSelection() {
        if (mCallback != null) mCallback.onResult(this);
    }

    /** Builder for the {@link UserInfoField}. */
    public static final class Builder {
        private String mDisplayText;
        private String mTextToFill;
        private String mA11yDescription;
        private String mId;
        private int mIconId;
        private boolean mIsObfuscated;
        private Callback<UserInfoField> mCallback;

        public Builder setDisplayText(String displayText) {
            this.mDisplayText = displayText;
            return this;
        }

        public Builder setTextToFill(String textToFill) {
            this.mTextToFill = textToFill;
            return this;
        }

        public Builder setA11yDescription(String a11yDescription) {
            this.mA11yDescription = a11yDescription;
            return this;
        }

        public Builder setId(String id) {
            this.mId = id;
            return this;
        }

        public Builder setIconId(int iconId) {
            this.mIconId = iconId;
            return this;
        }

        public Builder setIsObfuscated(boolean isObfuscated) {
            this.mIsObfuscated = isObfuscated;
            return this;
        }

        public Builder setCallback(Callback<UserInfoField> callback) {
            this.mCallback = callback;
            return this;
        }

        public UserInfoField build() {
            return new UserInfoField(
                    mDisplayText,
                    mTextToFill,
                    mA11yDescription,
                    mId,
                    mIconId,
                    mIsObfuscated,
                    mCallback);
        }
    }
}
