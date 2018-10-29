// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.util.List;

/**
 * A container class for a view showing a contact in the Contacts Picker.
 */
public class ContactView extends SelectableItemView<ContactDetails> {
    // The length of the fade in animation (in ms).
    private static final int IMAGE_FADE_IN_DURATION = 150;

    // Our context.
    private Context mContext;

    // Our parent category.
    private PickerCategoryView mCategoryView;

    // Our selection delegate.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The details of the contact shown.
    private ContactDetails mContactDetails;

    // The image view containing the profile image of the contact, or the abbreviated letters of the
    // contact's name.
    private ImageView mImage;

    // The control that signifies the contact has been selected.
    private ImageView mSelectedView;

    // The display name of the contact.
    public TextView mDisplayName;

    // The contact details for the contact.
    public TextView mDetailsView;

    /**
     * Constructor for inflating from XML.
     */
    public ContactView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mImage = (ImageView) findViewById(R.id.image);
        mDisplayName = (TextView) findViewById(R.id.name);
        mDetailsView = (TextView) findViewById(R.id.details);
        mSelectedView = (ImageView) findViewById(R.id.selected);
    }

    @Override
    public void onClick() {
        // Clicks are disabled until initialize() has been called.
        if (mContactDetails == null) return;

        // The SelectableItemView expects long press to be the selection event, but this class wants
        // that to happen on click instead.
        onLongClick(this);
    }

    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);
        updateSelectionState();
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        // If the user cancels the dialog before this object has initialized, the SelectionDelegate
        // will try to notify us that all selections have been cleared. However, we don't need to
        // process that message.
        if (mContactDetails == null) return;

        // When SelectAll or Undo is used, the underlying UI must be updated
        // to reflect the changes.
        boolean selected = selectedItems.contains(mContactDetails);
        boolean checked = super.isChecked();
        if (selected != checked) super.toggle();

        updateSelectionState();
    }

    /**
     * Sets the {@link PickerCategoryView} for this ContactView.
     * @param categoryView The category view showing the images. Used to access
     *     common functionality and sizes and retrieve the {@link SelectionDelegate}.
     */
    public void setCategoryView(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
        mSelectionDelegate = mCategoryView.getSelectionDelegate();
        setSelectionDelegate(mSelectionDelegate);
    }

    /**
     * Completes the initialization of the ContactView. Must be called before the
     * {@link ContactView} can respond to click events.
     * @param contactDetails The details about the contact represented by this ContactView.
     * @param icon The icon to show for the contact (or null if not loaded yet).
     */
    public void initialize(ContactDetails contactDetails, Bitmap icon) {
        resetTile();

        mContactDetails = contactDetails;
        setItem(contactDetails);

        String displayName = contactDetails.getDisplayName();
        mDisplayName.setText(displayName);
        mDetailsView.setText(contactDetails.getContactDetailsAsString());
        if (icon == null) {
            icon = mCategoryView.getIconGenerator().generateIconForText(
                    contactDetails.getDisplayNameAbbreviation(), 2);
            mImage.setImageBitmap(icon);
        } else {
            setIconBitmap(icon);
        }

        updateSelectionState();
    }

    /**
     * Sets the icon to display for the contact and fade it into view.
     * @param icon The icon to display.
     */
    public void setIconBitmap(Bitmap icon) {
        Resources resources = mContext.getResources();
        RoundedBitmapDrawable drawable = RoundedBitmapDrawableFactory.create(resources, icon);
        drawable.setCircular(true);
        mImage.setImageDrawable(drawable);
        mImage.setAlpha(0.0f);
        mImage.animate().alpha(1.0f).setDuration(IMAGE_FADE_IN_DURATION).start();
    }

    /**
     * Resets the view to its starting state, which is necessary when the view is about to be
     * re-used.
     */
    private void resetTile() {
        mImage.setImageBitmap(null);
        mDisplayName.setText("");
        mDetailsView.setText("");
        mSelectedView.setVisibility(View.GONE);
    }

    /**
     * Updates the selection controls for this view.
     */
    private void updateSelectionState() {
        boolean checked = super.isChecked();

        if (checked) {
            Resources resources = mContext.getResources();
            setBackgroundColor(ApiCompatibilityUtils.getColor(
                    resources, R.color.selectable_list_item_highlight_color));
        } else {
            setBackgroundColor(Color.TRANSPARENT);
        }

        mSelectedView.setVisibility(checked ? View.VISIBLE : View.GONE);
    }
}
