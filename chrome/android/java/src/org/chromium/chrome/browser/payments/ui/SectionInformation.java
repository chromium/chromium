// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.autofill.EditableOption;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * The data to show in a single section where the user can select something, e.g., their
 * shipping address or payment method.
 */
public class SectionInformation {
    /** This value indicates that the user has not made a selection in this section. */
    public static final int NO_SELECTION = -1;

    /** This value indicates that user selection is invalid in this section. */
    public static final int INVALID_SELECTION = -2;

    protected final ArrayList<EditableOption> mItems = new ArrayList<>();

    @PaymentRequestUI.DataType private final int mDataType;

    private int mSelectedItem;
    private boolean mDisplayInSingleLineInNormalMode = true;
    public String mErrorMessage;
    @Nullable public String mAddditionalText;

    /** Builds an empty section without selection. */
    public SectionInformation(@PaymentRequestUI.DataType int sectionType) {
        this(sectionType, null);
    }

    /**
     * Builds a section with a single option, which is selected.
     *
     * @param defaultItem The only item. It is selected by default.
     */
    public SectionInformation(
            @PaymentRequestUI.DataType int sectionType, @Nullable EditableOption defaultItem) {
        this(sectionType, 0, defaultItem == null ? null : Arrays.asList(defaultItem));
    }

    /**
     * Builds a section.
     *
     * @param sectionType    Type of data being stored.
     * @param selection      The index of the currently selected item.
     * @param itemCollection The items in the section.
     */
    public SectionInformation(
            @PaymentRequestUI.DataType int sectionType,
            int selection,
            Collection<? extends EditableOption> itemCollection) {
        mDataType = sectionType;
        updateItemsWithCollection(selection, itemCollection);
    }

    /**
     * Returns the data type contained in this section.
     *
     * @return The data type contained in this section.
     */
    public int getDataType() {
        return mDataType;
    }

    /**
     * Returns whether the section is empty.
     *
     * @return Whether the section is empty.
     */
    public boolean isEmpty() {
        return mItems.isEmpty();
    }

    /**
     * Returns the number of items in this section. For example, the number of shipping addresses or
     * payment methods.
     *
     * @return The number of items in this section.
     */
    public int getSize() {
        return mItems.size();
    }

    /**
     * Returns the item in the given position.
     *
     * @param position The index of the item to return.
     * @return The item in the given position or null.
     */
    public @Nullable EditableOption getItem(int position) {
        if (mItems.isEmpty() || position < 0 || position >= mItems.size()) {
            return null;
        }

        return mItems.get(position);
    }

    /**
     * Sets the currently selected item by index.
     *
     * @param index The index of the currently selected item, NO_SELECTION if a selection has not
     *              yet been made, or INVALID_SELECTION if an invalid selection has been made.
     */
    public void setSelectedItemIndex(int index) {
        mSelectedItem = index;
    }

    /**
     * Sets the currently selected item.
     *
     * @param selectedItem The currently selected item, or null of a selection has not yet been
     *                     made.
     */
    public void setSelectedItem(EditableOption selectedItem) {
        for (int i = 0; i < mItems.size(); i++) {
            if (mItems.get(i) == selectedItem) {
                mSelectedItem = i;
                return;
            }
        }
    }

    /**
     * Returns the index of the selected item.
     *
     * @return The index of the currently selected item, NO_SELECTION if a selection has not yet
     *         been made, or INVALID_SELECTION if an invalid selection has been made.
     */
    public int getSelectedItemIndex() {
        return mSelectedItem;
    }

    /**
     * Returns the selected item, if any.
     *
     * @return The selected item or null if none selected.
     */
    public @Nullable EditableOption getSelectedItem() {
        return getItem(getSelectedItemIndex());
    }

    /**
     * Adds the given item at the head of the list and selects it.
     *
     * @param item The item to add.
     */
    public void addAndSelectItem(EditableOption item) {
        mItems.add(0, item);
        mSelectedItem = 0;
    }

    /**
     * Adds the given item at the head of the list if it doesn't exist and selects it if it is
     * complete, otherwise updates the corresponding item and unselect it if it is incomplete.
     *
     * @param item The item to add or update.
     */
    public void addAndSelectOrUpdateItem(EditableOption item) {
        int i = 0;
        for (; i < mItems.size(); i++) {
            if (TextUtils.equals(mItems.get(i).getIdentifier(), item.getIdentifier())) {
                break;
            }
        }
        if (i < mItems.size()) {
            mItems.set(i, item);
            if (mSelectedItem == i && !item.isComplete()) mSelectedItem = NO_SELECTION;
            return;
        }

        mItems.add(0, item);
        if (item.isComplete()) {
            mSelectedItem = 0;
        } else {
            mSelectedItem = NO_SELECTION;
        }
    }

    /**
     * Remove the given item and unselect it if it is the selected item. Sets selected item to
     * INVALID_SELECTION if there is no item in this section.
     *
     * @param identifier The identifier of the removed item.
     */
    public void removeAndUnselectItem(String identifier) {
        for (int i = 0; i < mItems.size(); i++) {
            if (TextUtils.equals(mItems.get(i).getIdentifier(), identifier)) {
                if (mSelectedItem == i) {
                    mSelectedItem = NO_SELECTION;
                } else if (mSelectedItem > 0) {
                    // Update the selected item index.
                    mSelectedItem -= mSelectedItem > i ? 1 : 0;
                }
                mItems.remove(i);
                if (mItems.size() == 0) mSelectedItem = INVALID_SELECTION;
                break;
            }
        }
    }

    /**
     * Returns the resource ID for the string telling users that they can add a new option.
     *
     * @return ID if the user can add a new option, or 0 if they can't.
     */
    public int getAddStringId() {
        if (mDataType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            return R.string.payments_add_address;
        } else if (mDataType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            return R.string.payments_add_contact;
        } else if (mDataType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            return R.string.payments_add_card;
        }
        return 0;
    }

    /**
     * Returns the resource ID for generating string to preview options in this section.
     *
     * @return The resource ID.
     */
    public int getPreviewStringResourceId() {
        switch (mDataType) {
            case PaymentRequestUI.DataType.SHIPPING_ADDRESSES:
                return R.plurals.payment_request_shipping_addresses_preview;
            case PaymentRequestUI.DataType.SHIPPING_OPTIONS:
                return R.plurals.payment_request_shipping_options_preview;
            case PaymentRequestUI.DataType.PAYMENT_METHODS:
                return R.plurals.payment_request_payment_methods_preview;
            case PaymentRequestUI.DataType.CONTACT_DETAILS:
                return R.plurals.payment_request_contacts_preview;
            default:
                assert false : "unknown data type";
                return 0;
        }
    }

    /** @param msg The optional error message to display when the selection is invalid. */
    public void setErrorMessage(String msg) {
        mErrorMessage = msg;
    }

    /** @return The optional error message to display when the selection is invalid. */
    public String getErrorMessage() {
        return mErrorMessage;
    }

    /** @param text The optional additional text to display in this section. */
    public void setAdditionalText(String text) {
        mAddditionalText = text;
    }

    /** @return The optional additional text to display in this section. */
    public @Nullable String getAdditionalText() {
        return mAddditionalText;
    }

    /** @return List of items in the section. */
    public List<EditableOption> getItems() {
        return mItems;
    }

    /**
     * Update the list of items being shown by this section, as well as the selection.
     *
     * @param selection      The index of the currently selected item.
     * @param itemCollection The items in the section.
     */
    protected void updateItemsWithCollection(
            int selection, @Nullable Collection<? extends EditableOption> itemCollection) {
        mItems.clear();
        if (itemCollection == null || itemCollection.isEmpty()) {
            mSelectedItem = NO_SELECTION;
        } else {
            mSelectedItem = selection;
            mItems.addAll(itemCollection);
        }
    }

    /**
     * Set whether display the selected item summary in single line in
     * PaymentRequestSection.DISPLAY_MODE_NORMAL.
     *
     * @param singleLine whether display in single line, note that the default value is true.
     */
    public void setDisplaySelectedItemSummaryInSingleLineInNormalMode(boolean singleLine) {
        mDisplayInSingleLineInNormalMode = singleLine;
    }

    /**
     * Get whether display the selected item summary in single line in
     * PaymentRequestSection.DISPLAY_MODE_NORMAL.
     *
     */
    public boolean getDisplaySelectedItemSummaryInSingleLineInNormalMode() {
        return mDisplayInSingleLineInNormalMode;
    }
}
