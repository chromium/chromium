// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

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

import org.chromium.chrome.R;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel.DropdownKeyValue;
import org.chromium.components.autofill.prefeditor.EditorFieldView;
import org.chromium.components.autofill.prefeditor.EditorObserverForTest;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class for creating a dropdown view with a label.
 */
class EditorDropdownField implements EditorFieldView {
    @Nullable
    private static EditorObserverForTest sObserverForTest;

    private final Context mContext;
    private final EditorFieldModel mFieldModel;
    private final View mLayout;
    private final TextView mLabel;
    private final Spinner mDropdown;
    private final View mUnderline;
    private final TextView mErrorLabel;
    private int mSelectedIndex;
    private ArrayAdapter<CharSequence> mAdapter;

    /**
     * Builds a dropdown view.
     *
     * @param context              The application context to use when creating widgets.
     * @param root                 The object that provides a set of LayoutParams values for
     *                             the view.
     * @param fieldModel           The data model of the dropdown.
     * @param changedCallback      The callback to invoke after user's dropdwn item selection has
     *                             been processed.
     * @param hasRequiredIndicator Whether the required (*) indicator is visible.
     */
    public EditorDropdownField(Context context, ViewGroup root, final EditorFieldModel fieldModel,
            final Runnable changedCallback, boolean hasRequiredIndicator) {
        assert fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN;
        mContext = context;
        mFieldModel = fieldModel;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.payment_request_editor_dropdown, root, false);

        mLabel = (TextView) mLayout.findViewById(R.id.spinner_label);
        mLabel.setText(mFieldModel.isRequired() && hasRequiredIndicator
                        ? mFieldModel.getLabel() + EditorDialog.REQUIRED_FIELD_INDICATOR
                        : mFieldModel.getLabel());

        mUnderline = mLayout.findViewById(R.id.spinner_underline);

        mErrorLabel = mLayout.findViewById(R.id.spinner_error);

        final List<DropdownKeyValue> dropdownKeyValues = mFieldModel.getDropdownKeyValues();
        final List<CharSequence> dropdownValues = getDropdownValues(dropdownKeyValues);
        if (mFieldModel.getHint() != null) {
            // Pass the hint to be displayed as default. If there is a '+' icon needed, use the
            // appropriate class to display it.
            if (mFieldModel.isDisplayPlusIcon()) {
                mAdapter = new HintedDropDownAdapterWithPlusIcon<CharSequence>(context,
                        R.layout.multiline_spinner_item, R.id.spinner_item, dropdownValues,
                        mFieldModel.getHint().toString());
            } else {
                mAdapter = new HintedDropDownAdapter<CharSequence>(context,
                        R.layout.multiline_spinner_item, R.id.spinner_item, dropdownValues,
                        mFieldModel.getHint().toString());
            }
            // Wrap the TextView in the dropdown popup around with a FrameLayout to display the text
            // in multiple lines.
            // Note that the TextView in the dropdown popup is displayed in a DropDownListView for
            // the dropdown style Spinner and the DropDownListView sets to display TextView instance
            // in a single line.
            mAdapter.setDropDownViewResource(R.layout.payment_request_dropdown_item);
        } else {
            mAdapter = new DropdownFieldAdapter<CharSequence>(
                    context, R.layout.multiline_spinner_item, dropdownValues);
            mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        }

        // If no value is selected or the value previously entered is not valid, we'll  select the
        // hint, which is the first item on the dropdown adapter. We also need to check for both key
        // and value, because the saved value could take both forms (as in NY or New York).
        mSelectedIndex = TextUtils.isEmpty(mFieldModel.getValue())
                ? 0
                : mAdapter.getPosition(mFieldModel.getValue().toString());
        if (mSelectedIndex < 0) {
            // Assuming that mFieldModel.getValue() is the value (New York).
            mSelectedIndex = mAdapter.getPosition(
                    mFieldModel.getDropdownValueByKey(mFieldModel.getValue().toString()));
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
                    String key = mFieldModel.getDropdownKeyByValue(mAdapter.getItem(position));
                    // If the hint is selected, it means that no value is entered by the user.
                    if (mFieldModel.getHint() != null && position == 0) {
                        key = null;
                    }
                    mSelectedIndex = position;
                    mFieldModel.setDropdownKey(key, changedCallback);
                    mFieldModel.setCustomErrorMessage(null);
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

    /** @return The EditorFieldModel that the EditorDropdownField represents. */
    public EditorFieldModel getFieldModel() {
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
        return mFieldModel.isValid();
    }

    @Override
    public boolean isRequired() {
        return mFieldModel.isRequired();
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
                ((TextView) view).setError(mFieldModel.getErrorMessage(), drawable);
                mUnderline.setBackgroundColor(mContext.getColor(R.color.default_text_color_error));
                mErrorLabel.setText(mFieldModel.getErrorMessage());
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
        mSelectedIndex = TextUtils.isEmpty(mFieldModel.getValue())
                ? 0
                : mAdapter.getPosition(
                        mFieldModel.getDropdownValueByKey((mFieldModel.getValue().toString())));
        assert mSelectedIndex >= 0;
        mDropdown.setSelection(mSelectedIndex);
    }

    private static List<CharSequence> getDropdownValues(List<DropdownKeyValue> dropdownKeyValues) {
        List<CharSequence> dropdownValues = new ArrayList<CharSequence>();
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
