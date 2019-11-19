// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.support.v7.app.AlertDialog;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.ui.ContactsPickerListener;

/**
 * UI for the contacts picker that shows on the Android platform as a result of
 * &lt;input type=file accept=contacts &gt; form element.
 */
public class ContactsPickerDialog
        extends AlertDialog implements ContactsPickerToolbar.ContactsToolbarDelegate {
    // The category we're showing contacts for.
    private PickerCategoryView mCategoryView;

    /**
     * The ContactsPickerDialog constructor.
     * @param context The context to use.
     * @param listener The listener object that gets notified when an action is taken.
     * @param allowMultiple Whether the contacts picker should allow multiple items to be selected.
     * @param includeNames Whether the contacts data returned should include names.
     * @param includeEmails Whether the contacts data returned should include emails.
     * @param includeTel Whether the contacts data returned should include telephone numbers.
     * @param includeAddresses Whether the contacts data returned should include addresses.
     * @param includeIcons Whether the contacts data returned should include icons.
     * @param formattedOrigin The origin the data will be shared with, formatted for display with
     *                        the scheme omitted.
     */
    public ContactsPickerDialog(Context context, ContactsPickerListener listener,
            boolean allowMultiple, boolean includeNames, boolean includeEmails, boolean includeTel,
            boolean includeAddresses, boolean includeIcons, String formattedOrigin) {
        super(context, R.style.Theme_Chromium_Fullscreen);

        // Initialize the main content view.
        mCategoryView = new PickerCategoryView(context, allowMultiple, includeNames, includeEmails,
                includeTel, includeAddresses, includeIcons, formattedOrigin, this);
        mCategoryView.initialize(this, listener);
        setView(mCategoryView);
    }

    /**
     * Cancels the dialog in response to a back navigation.
     */
    @Override
    public void onNavigationBackCallback() {
        cancel();
    }

    @VisibleForTesting
    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
