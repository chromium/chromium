// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

/** Immutable holder for displayable profile data. */
public class DisplayableProfileData {
    private final String mAccountEmail;
    private final Drawable mImage;
    private final @Nullable String mFullName;
    private final @Nullable String mGivenName;
    private final boolean mHasDisplayableEmailAddress;

    public DisplayableProfileData(
            String accountEmail,
            Drawable image,
            @Nullable String fullName,
            @Nullable String givenName,
            boolean hasDisplayableEmailAddress) {
        assert accountEmail != null;
        assert image != null;
        mAccountEmail = accountEmail;
        mImage = image;
        mFullName = fullName;
        mGivenName = givenName;
        mHasDisplayableEmailAddress = hasDisplayableEmailAddress;
    }

    /**
     * @return The account email.
     */
    public String getAccountEmail() {
        return mAccountEmail;
    }

    /**
     * @return The image of the account if it is available or a placeholder image otherwise.
     */
    public Drawable getImage() {
        return mImage;
    }

    /**
     * @return The full name of the user (e.g., "John Doe").
     */
    public @Nullable String getFullName() {
        return mFullName;
    }

    /**
     * @return The given name of the user (e.g., "John" from "John Doe").
     */
    public @Nullable String getGivenName() {
        return mGivenName;
    }

    /**
     * @return The full name of the user if it is available or the email otherwise.
     */
    public String getFullNameOrEmail() {
        if (mFullName == null) {
            return mAccountEmail;
        }
        return mFullName;
    }

    /**
     * Returns the given name of the user if it is available or the full name or email otherwise.
     */
    public String getGivenNameOrFullNameOrEmail() {
        if (mGivenName != null) {
            return mGivenName;
        }
        return getFullNameOrEmail();
    }

    /**
     * @return Whether the account email address can be displayed.
     */
    public boolean hasDisplayableEmailAddress() {
        return mHasDisplayableEmailAddress;
    }
}
