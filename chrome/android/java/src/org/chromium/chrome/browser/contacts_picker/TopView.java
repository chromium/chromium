// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChipView;

import java.text.NumberFormat;

/**
 * A container class for the Disclaimer and Select All functionality (and both associated labels).
 */
public class TopView extends RelativeLayout
        implements CompoundButton.OnCheckedChangeListener, View.OnClickListener {
    /**
     * An interface for communicating when the Select All checkbox is toggled.
     */
    public interface SelectAllToggleCallback {
        /**
         * Called when the Select All checkbox is toggled.
         * @param allSelected Whether the Select All checkbox is checked.
         */
        void onSelectAllToggled(boolean allSelected);
    }

    /**
     * An interface for communicating when one of the chips has been toggled.
     */
    public interface ChipToggledCallback {
        /**
         * Called when a Chip is toggled.
         * @param chip The chip type that was toggled.
         */
        void onChipToggled(@PickerAdapter.FilterType int chip);
    }

    private final Context mContext;

    // The container box for the checkbox and its label and contact count.
    private View mCheckboxContainer;

    // The Select All checkbox.
    private CheckBox mSelectAllBox;

    // The label showing how many contacts were found.
    private TextView mContactCount;

    // The callback to use when notifying that the Select All checkbox was toggled.
    private SelectAllToggleCallback mSelectAllCallback;

    // A Chip for filtering out names.
    private ChipView mNamesFilterChip;

    // A Chip for filtering out addresses.
    private ChipView mAddressFilterChip;

    // A Chip for filtering out emails.
    private ChipView mEmailFilterChip;

    // A Chip for filtering out telephones.
    private ChipView mTelephonesFilterChip;

    // A Chip for filtering out telephones.
    private ChipView mIconsFilterChip;

    // The callback to use to notify when the filter chips are toggled.
    private ChipToggledCallback mChipToggledCallback;

    // Whether to temporarily ignore clicks on the checkbox.
    private boolean mIgnoreCheck;

    public TopView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCheckboxContainer = findViewById(R.id.content);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTACTS_PICKER_SELECT_ALL)) {
            mCheckboxContainer.setVisibility(View.VISIBLE);
        }
        mSelectAllBox = findViewById(R.id.checkbox);
        mContactCount = findViewById(R.id.checkbox_details);

        TextView title = findViewById(R.id.checkbox_title);
        title.setText(R.string.contacts_picker_all_contacts);

        mNamesFilterChip = findViewById(R.id.names_filter);
        TextView textView = mNamesFilterChip.getPrimaryTextView();
        textView.setText(R.string.top_view_names_filter_label);
        mNamesFilterChip.setSelected(true);
        mNamesFilterChip.setOnClickListener(this);
        mNamesFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, false);

        mAddressFilterChip = findViewById(R.id.address_filter);
        textView = mAddressFilterChip.getPrimaryTextView();
        textView.setText(R.string.top_view_address_filter_label);
        mAddressFilterChip.setSelected(true);
        mAddressFilterChip.setOnClickListener(this);
        mAddressFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, false);

        mEmailFilterChip = findViewById(R.id.email_filter);
        textView = mEmailFilterChip.getPrimaryTextView();
        textView.setText(R.string.top_view_email_filter_label);
        mEmailFilterChip.setSelected(true);
        mEmailFilterChip.setOnClickListener(this);
        mEmailFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, false);

        mTelephonesFilterChip = findViewById(R.id.tel_filter);
        textView = mTelephonesFilterChip.getPrimaryTextView();
        textView.setText(R.string.top_view_telephone_filter_label);
        mTelephonesFilterChip.setSelected(true);
        mTelephonesFilterChip.setOnClickListener(this);
        mTelephonesFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, false);

        mIconsFilterChip = findViewById(R.id.icon_filter);
        textView = mIconsFilterChip.getPrimaryTextView();
        textView.setText(R.string.top_view_icon_filter_label);
        mIconsFilterChip.setSelected(true);
        mIconsFilterChip.setOnClickListener(this);
        mIconsFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, false);
    }

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.names_filter) {
            notifyChipToggled(PickerAdapter.FilterType.NAMES);
        } else if (id == R.id.address_filter) {
            notifyChipToggled(PickerAdapter.FilterType.ADDRESSES);
        } else if (id == R.id.email_filter) {
            notifyChipToggled(PickerAdapter.FilterType.EMAILS);
        } else if (id == R.id.tel_filter) {
            notifyChipToggled(PickerAdapter.FilterType.TELEPHONES);
        } else if (id == R.id.icon_filter) {
            notifyChipToggled(PickerAdapter.FilterType.ICONS);
        }
    }

    /**
     * Sends a notification that a chip has been toggled and updates the selection state for it.
     * @param chip The id of the chip that was toggled.
     */
    public void notifyChipToggled(@PickerAdapter.FilterType int chip) {
        ChipView chipView;
        int iconResId = 0;

        switch (chip) {
            case PickerAdapter.FilterType.NAMES:
                chipView = mNamesFilterChip;
                iconResId = R.drawable.names;
                break;
            case PickerAdapter.FilterType.ADDRESSES:
                chipView = mAddressFilterChip;
                iconResId = R.drawable.address;
                break;
            case PickerAdapter.FilterType.EMAILS:
                chipView = mEmailFilterChip;
                iconResId = R.drawable.email;
                break;
            case PickerAdapter.FilterType.TELEPHONES:
                chipView = mTelephonesFilterChip;
                iconResId = R.drawable.telephone;
                break;
            case PickerAdapter.FilterType.ICONS:
                chipView = mIconsFilterChip;
                iconResId = R.drawable.face;
                break;
            default:
                assert false;
                return;
        }

        chipView.setSelected(!chipView.isSelected());
        chipView.setIcon(
                chipView.isSelected() ? R.drawable.ic_check_googblue_24dp : iconResId, true);
        mChipToggledCallback.onChipToggled(chip);
    }

    /**
     * Set the string explaining which site the dialog will be sharing the data with.
     * @param origin The origin string to display.
     */
    public void setSiteString(String origin) {
        TextView explanation = findViewById(R.id.explanation);
        StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        explanation.setText(SpanApplier.applySpans(
                mContext.getString(R.string.disclaimer_sharing_contact_details, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
    }

    /**
     * Register a callback to use to notify that Select All was toggled.
     * @param callback The callback to use.
     */
    public void registerSelectAllCallback(SelectAllToggleCallback callback) {
        mSelectAllCallback = callback;
    }

    /**
     * Register a callback to use to notify when the filter chips are toggled.
     */
    public void registerChipToggledCallback(ChipToggledCallback callback) {
        mChipToggledCallback = callback;
    }

    /**
     * Updates the visibility of the Select All checkbox.
     * @param visible Whether the checkbox should be visible.
     */
    public void updateCheckboxVisibility(boolean visible) {
        if (visible) {
            mSelectAllBox.setOnCheckedChangeListener(this);
        } else {
            mCheckboxContainer.setVisibility(GONE);
        }
    }

    /**
     * Updates which chips should be displayed as part of the top view.
     * @param shouldDisplayNames Whether the names chip should be displayed.
     * @param shouldDisplayAddresses Whether the addresses chip should be displayed.
     * @param shouldDisplayEmails Whether the emails chip should be displayed.
     * @param shouldDisplayTel Whether the telephone chip should be displayed.
     */
    public void updateChipVisibility(boolean shouldDisplayNames, boolean shouldDisplayAddresses,
            boolean shouldDisplayEmails, boolean shouldDisplayTel, boolean shouldDisplayIcons) {
        mNamesFilterChip.setVisibility(shouldDisplayNames ? View.VISIBLE : View.GONE);
        mAddressFilterChip.setVisibility(shouldDisplayAddresses ? View.VISIBLE : View.GONE);
        mEmailFilterChip.setVisibility(shouldDisplayEmails ? View.VISIBLE : View.GONE);
        mTelephonesFilterChip.setVisibility(shouldDisplayTel ? View.VISIBLE : View.GONE);
        mIconsFilterChip.setVisibility(shouldDisplayIcons ? View.VISIBLE : View.GONE);
    }

    /**
     * Updates the total number of contacts found in the dialog.
     * @param count The number of contacts found.
     */
    public void updateContactCount(int count) {
        mContactCount.setText(NumberFormat.getInstance().format(count));
    }

    /**
     * Toggles the Select All checkbox.
     */
    public void toggle() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTACTS_PICKER_SELECT_ALL)) {
            mSelectAllBox.setChecked(!mSelectAllBox.isChecked());
        }
    }

    /**
     * Returns how many filter chips are checked.
     */
    public int filterChipsChecked() {
        int checked = 0;
        if (mNamesFilterChip.getVisibility() == View.VISIBLE && mNamesFilterChip.isSelected()) {
            ++checked;
        }
        if (mAddressFilterChip.getVisibility() == View.VISIBLE && mAddressFilterChip.isSelected()) {
            ++checked;
        }
        if (mEmailFilterChip.getVisibility() == View.VISIBLE && mEmailFilterChip.isSelected()) {
            ++checked;
        }
        if (mTelephonesFilterChip.getVisibility() == View.VISIBLE
                && mTelephonesFilterChip.isSelected()) {
            ++checked;
        }
        if (mIconsFilterChip.getVisibility() == View.VISIBLE && mIconsFilterChip.isSelected()) {
            ++checked;
        }
        return checked;
    }

    /**
     * Updates the state of the checkbox to reflect whether everything is selected.
     * @param allSelected
     */
    public void updateSelectAllCheckbox(boolean allSelected) {
        mIgnoreCheck = true;
        mSelectAllBox.setChecked(allSelected);
        mIgnoreCheck = false;
    }

    @Override
    public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
        if (!mIgnoreCheck) mSelectAllCallback.onSelectAllToggled(mSelectAllBox.isChecked());
    }
}
