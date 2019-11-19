// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.support.v4.widget.ImageViewCompat;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/**
 * Handles toolbar functionality for the {@ContactsPickerDialog}.
 */
public class ContactsPickerToolbar extends SelectableListToolbar<ContactDetails> {
    /**
     * A delegate that handles dialog actions.
     */
    public interface ContactsToolbarDelegate {
        /**
         * Called when the back arrow is clicked in the toolbar.
         */
        void onNavigationBackCallback();
    }

    // A delegate to notify when the dialog should close.
    ContactsToolbarDelegate mDelegate;
    public ContactsPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the {@ContactToolbarDelegate} for this toolbar.
     */
    public void setDelegate(ContactsToolbarDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Shows the Back arrow navigation button in the upper left corner.
     */
    public void showBackArrow() {
        setNavigationButton(NAVIGATION_BUTTON_BACK);
    }

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
        } else {
            mDelegate.onNavigationBackCallback();
        }
    }

    @Override
    public void initialize(SelectionDelegate<ContactDetails> delegate, int titleResId,
            int normalGroupResId, int selectedGroupResId, boolean updateStatusBarColor) {
        super.initialize(
                delegate, titleResId, normalGroupResId, selectedGroupResId, updateStatusBarColor);

        showBackArrow();
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        super.onSelectionStateChange(selectedItems);

        int selectCount = selectedItems.size();
        ButtonCompat done = findViewById(R.id.done);
        done.setEnabled(selectCount > 0);

        AppCompatImageView search = findViewById(R.id.search);
        ImageViewCompat.setImageTintList(search,
                useDarkIcons() ? getDarkIconColorStateList() : getLightIconColorStateList());

        if (selectCount > 0) {
            ApiCompatibilityUtils.setTextAppearance(done, R.style.TextAppearance_Body_Inverse);
        } else {
            ApiCompatibilityUtils.setTextAppearance(
                    done, R.style.TextAppearance_BlackDisabledText3);

            showBackArrow();
        }
    }
}
