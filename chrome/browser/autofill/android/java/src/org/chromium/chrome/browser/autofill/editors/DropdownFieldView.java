// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.CUSTOM_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getDropdownKeyByValue;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getDropdownValueByKey;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getValidationErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.isFieldValid;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.setDropdownKey;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class for creating a dropdown view with a label.
 */
class DropdownFieldView implements FieldView {
    @Nullable
    private static EditorObserverForTest sObserverForTest;

    private final Context mContext;
    private final PropertyModel mFieldModel;
    private final View mLayout;
    private final TextView mLabel;
    private final Spinner mDropdown;
    private final View mUnderline;
    private final TextView mErrorLabel;
    private int mSelectedIndex;
    private ArrayAdapter<String> mAdapter;

    /**
     * Builds a dropdown view.
     *
     * @param context              The application context to use when creating widgets.
     * @param root                 The object that provides a set of LayoutParams values for
     *                             the view.
     * @param fieldModel           The data model of the dropdown.
     * @param hasRequiredIndicator Whether the required (*) indicator is visible.
     */
    public DropdownFieldView(Context context, ViewGroup root, final PropertyModel fieldModel,
            boolean hasRequiredIndicator) {
        mContext = context;
        mFieldModel = fieldModel;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.payment_request_editor_dropdown, root, false);

        mLabel = (TextView) mLayout.findViewById(R.id.spinner_label);
        mLabel.setText(mFieldModel.get(IS_REQUIRED) && hasRequiredIndicator
                        ? mFieldModel.get(LABEL) + EditorDialogView.REQUIRED_FIELD_INDICATOR
                        : mFieldModel.get(LABEL));

        mUnderline = mLayout.findViewById(R.id.spinner_underline);

        mErrorLabel = mLayout.findViewById(R.id.spinner_error);

        final List<DropdownKeyValue> dropdownKeyValues = mFieldModel.get(DROPDOWN_KEY_VALUE_LIST);
        final List<String> dropdownValues = getDropdownValues(dropdownKeyValues);
        if (mFieldModel.get(DROPDOWN_HINT) != null) {
            mAdapter = new HintedDropDownAdapter<String>(context, R.layout.multiline_spinner_item,
                    R.id.spinner_item, dropdownValues, mFieldModel.get(DROPDOWN_HINT));
            // Wrap the TextView in the dropdown popup around with a FrameLayout to display the text
            // in multiple lines.
            // Note that the TextView in the dropdown popup is displayed in a DropDownListView for
            // the dropdown style Spinner and the DropDownListView sets to display TextView instance
            // in a single line.
            mAdapter.setDropDownViewResource(R.layout.payment_request_dropdown_item);
        } else {
            mAdapter = new DropdownFieldAdapter<String>(
                    context, R.layout.multiline_spinner_item, dropdownValues);
            mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        }

        // If no value is selected or the value previously entered is not valid, we'll  select the
        // hint, which is the first item on the dropdown adapter. We also need to check for both key
        // and value, because the saved value could take both forms (as in NY or New York).
        mSelectedIndex = TextUtils.isEmpty(mFieldModel.get(VALUE))
                ? 0
                : mAdapter.getPosition(mFieldModel.get(VALUE));
        if (mSelectedIndex < 0) {
            // Assuming that mFieldModel.get(VALUE) is the value (New York).
            mSelectedIndex = mAdapter.getPosition(
                    getDropdownValueByKey(mFieldModel, mFieldModel.get(VALUE)));
        }
        // Invalid value in the mFieldModel
        if (mSelectedIndex < 0) mSelectedIndex = 0;

        mDropdown = (Spinner) mLayout.findViewById(R.id.spinner);
        mDropdown.setTag(this);
        mDropdown.setAdapter(mAdapter);
        mDropdown.setSelection(mSelectedIndex);
        mDropdown.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (mSelectedIndex != position) {
                    String key = getDropdownKeyByValue(mFieldModel, mAdapter.getItem(position));
                    // If the hint is selected, it means that no value is entered by the user.
                    if (mFieldModel.get(DROPDOWN_HINT) != null && position == 0) {
                        key = null;
                    }
                    mSelectedIndex = position;
                    setDropdownKey(mFieldModel, key);
                    mFieldModel.set(CUSTOM_ERROR_MESSAGE, null);
                    updateDisplayedError(false);
                }
                if (sObserverForTest != null) {
                    sObserverForTest.onEditorTextUpdate();
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
        mDropdown.setOnTouchListener(new View.OnTouchListener() {
            @SuppressLint("ClickableViewAccessibility")
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) requestFocusAndHideKeyboard();

                return false;
            }
        });
    }

    /** @return The View containing everything. */
    public View getLayout() {
        return mLayout;
    }

    /** @return The PropertyModel that the DropdownFieldView represents. */
    public PropertyModel getFieldModel() {
        return mFieldModel;
    }

    /** @return The label view for the spinner. */
    public View getLabel() {
        return mLabel;
    }

    /** @return The dropdown view itself. */
    public Spinner getDropdown() {
        return mDropdown;
    }

    @Override
    public boolean isValid() {
        return isFieldValid(mFieldModel);
    }

    @Override
    public boolean isRequired() {
        return mFieldModel.get(IS_REQUIRED);
    }

    @Override
    public void updateDisplayedError(boolean showError) {
        View view = mDropdown.getSelectedView();
        if (view != null && view instanceof TextView) {
            if (showError) {
                Drawable drawable = TraceEventVectorDrawableCompat.create(
                        mContext.getResources(), R.drawable.ic_error, mContext.getTheme());
                drawable.setBounds(
                        0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
                ((TextView) view).setError(getValidationErrorMessage(mFieldModel), drawable);
                mUnderline.setBackgroundColor(mContext.getColor(R.color.default_text_color_error));
                mErrorLabel.setText(getValidationErrorMessage(mFieldModel));
                mErrorLabel.setVisibility(View.VISIBLE);
            } else {
                ((TextView) view).setError(null);
                mUnderline.setBackgroundColor(mContext.getColor(R.color.modern_grey_600));
                mErrorLabel.setText(null);
                mErrorLabel.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void scrollToAndFocus() {
        updateDisplayedError(!isValid());
        requestFocusAndHideKeyboard();
    }

    private void requestFocusAndHideKeyboard() {
        KeyboardVisibilityDelegate.getInstance().hideKeyboard(mDropdown);
        ViewGroup parent = (ViewGroup) mDropdown.getParent();
        if (parent != null) parent.requestChildFocus(mDropdown, mDropdown);
        mDropdown.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    public void update() {
        // If no value is selected, we'll  select the hint, which is the first item on the dropdown
        // adapter.
        mSelectedIndex = TextUtils.isEmpty(mFieldModel.get(VALUE))
                ? 0
                : mAdapter.getPosition(getDropdownValueByKey(mFieldModel, mFieldModel.get(VALUE)));
        assert mSelectedIndex >= 0;
        mDropdown.setSelection(mSelectedIndex);
    }

    private static List<String> getDropdownValues(List<DropdownKeyValue> dropdownKeyValues) {
        List<String> dropdownValues = new ArrayList<String>();
        for (int i = 0; i < dropdownKeyValues.size(); i++) {
            dropdownValues.add(dropdownKeyValues.get(i).getValue());
        }
        return dropdownValues;
    }

    @VisibleForTesting
    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }
}
