// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getDropdownKeyByValue;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getDropdownValueByKey;
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

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Helper class for creating a dropdown view with a label. */
class DropdownFieldView implements FieldView {
    @Nullable private static EditorObserverForTest sObserverForTest;

    private final Context mContext;
    private final PropertyModel mFieldModel;
    private final View mLayout;
    private final TextView mLabel;
    private final Spinner mDropdown;
    private final View mUnderline;
    private final TextView mErrorLabel;
    private int mSelectedIndex;
    private ArrayAdapter<String> mAdapter;
    @Nullable private String mHint;
    @Nullable private EditorFieldValidator mValidator;
    private boolean mShowRequiredIndicator;

    /**
     * Builds a dropdown view.
     *
     * @param context The application context to use when creating widgets.
     * @param root The object that provides a set of LayoutParams values for the view.
     * @param fieldModel The data model of the dropdown.
     */
    public DropdownFieldView(Context context, ViewGroup root, final PropertyModel fieldModel) {
        mContext = context;
        mFieldModel = fieldModel;

        mLayout =
                LayoutInflater.from(context)
                        .inflate(R.layout.payment_request_editor_dropdown, root, false);

        mLabel = (TextView) mLayout.findViewById(R.id.spinner_label);
        setShowRequiredIndicator(/* showRequiredIndicator= */ false);

        mUnderline = mLayout.findViewById(R.id.spinner_underline);

        mErrorLabel = mLayout.findViewById(R.id.spinner_error);

        mDropdown = (Spinner) mLayout.findViewById(R.id.spinner);
        mDropdown.setTag(this);
        mDropdown.setOnItemSelectedListener(
                new OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(
                            AdapterView<?> parent, View view, int position, long id) {
                        if (mSelectedIndex != position) {
                            String key =
                                    getDropdownKeyByValue(mFieldModel, mAdapter.getItem(position));
                            // If the hint is selected, it means that no value is entered by the
                            // user.
                            if (mHint != null && position == 0) {
                                key = null;
                            }
                            mSelectedIndex = position;
                            setDropdownKey(mFieldModel, key);

                            if (mValidator != null) {
                                mValidator.onUserEditedField();
                                mValidator.validate(mFieldModel);
                            }
                        }
                        if (sObserverForTest != null) {
                            sObserverForTest.onEditorTextUpdate();
                        }
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> parent) {}
                });
        mDropdown.setOnTouchListener(
                new View.OnTouchListener() {
                    @SuppressLint("ClickableViewAccessibility")
                    @Override
                    public boolean onTouch(View v, MotionEvent event) {
                        if (event.getAction() == MotionEvent.ACTION_DOWN) {
                            requestFocusAndHideKeyboard();
                        }

                        return false;
                    }
                });
    }

    void setLabel(String label, boolean isRequired) {
        mLabel.setText(
                isRequired && mShowRequiredIndicator
                        ? label + EditorDialogView.REQUIRED_FIELD_INDICATOR
                        : label);
    }

    void setDropdownValues(List<String> values, @Nullable String hint) {
        mHint = hint;
        if (mHint != null) {
            mAdapter =
                    new HintedDropDownAdapter<String>(
                            mContext,
                            R.layout.multiline_spinner_item,
                            R.id.spinner_item,
                            values,
                            mHint);
            // Wrap the TextView in the dropdown popup around with a FrameLayout to display the text
            // in multiple lines.
            // Note that the TextView in the dropdown popup is displayed in a DropDownListView for
            // the dropdown style Spinner and the DropDownListView sets to display TextView instance
            // in a single line.
            mAdapter.setDropDownViewResource(R.layout.payment_request_dropdown_item);
        } else {
            mAdapter =
                    new DropdownFieldAdapter<String>(
                            mContext, R.layout.multiline_spinner_item, values);
            mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        }
        mDropdown.setAdapter(mAdapter);
    }

    void setValue(@Nullable String value) {
        if (mAdapter == null) {
            // Can't set value when adapter hasn't been initialized.
            return;
        }
        // If no value is selected or the value previously entered is not valid, we'll  select the
        // hint, which is the first item on the dropdown adapter. We also need to check for both key
        // and value, because the saved value could take both forms (NY or New York).
        mSelectedIndex = TextUtils.isEmpty(value) ? 0 : mAdapter.getPosition(value);
        if (mSelectedIndex < 0) {
            // Assuming that value is the value (NY).
            mSelectedIndex = mAdapter.getPosition(getDropdownValueByKey(mFieldModel, value));
        }
        // Invalid value in the mFieldModel
        if (mSelectedIndex < 0) mSelectedIndex = 0;
        mDropdown.setSelection(mSelectedIndex);
    }

    void setErrorMessage(@Nullable String errorMessage) {
        View view = mDropdown.getSelectedView();
        if (errorMessage == null) {
            // {@link Spinner#getSelectedView()} is null in JUnit tests.
            if (view != null && view instanceof TextView) {
                ((TextView) view).setError(null);
            }
            mUnderline.setBackgroundColor(mContext.getColor(R.color.modern_grey_600));
            mErrorLabel.setText(null);
            mErrorLabel.setVisibility(View.GONE);
            return;
        }

        Drawable drawable =
                TraceEventVectorDrawableCompat.create(
                        mContext.getResources(), R.drawable.ic_error, mContext.getTheme());
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        if (view != null && view instanceof TextView) {
            ((TextView) view).setError(errorMessage, drawable);
        }
        mUnderline.setBackgroundColor(mContext.getColor(R.color.default_text_color_error));
        mErrorLabel.setText(errorMessage);
        mErrorLabel.setVisibility(View.VISIBLE);

        if (sObserverForTest != null) {
            sObserverForTest.onEditorValidationError();
        }
    }

    void setValidator(@Nullable EditorFieldValidator validator) {
        mValidator = validator;
    }

    @Override
    public void setShowRequiredIndicator(boolean showRequiredIndicator) {
        mShowRequiredIndicator = showRequiredIndicator;
        setLabel(mFieldModel.get(LABEL), mFieldModel.get(IS_REQUIRED));
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

    public TextView getErrorLabelForTests() {
        return mErrorLabel;
    }

    @Override
    public boolean validate() {
        if (mValidator != null) {
            mValidator.validate(mFieldModel);
        }
        return mFieldModel.get(ERROR_MESSAGE) == null;
    }

    @Override
    public boolean isRequired() {
        return mFieldModel.get(IS_REQUIRED);
    }

    @Override
    public void scrollToAndFocus() {
        requestFocusAndHideKeyboard();
    }

    private void requestFocusAndHideKeyboard() {
        // Clear 'focused' bit because dropdown is not focusable. (crbug.com/1474419)
        mFieldModel.set(FOCUSED, false);

        KeyboardVisibilityDelegate.getInstance().hideKeyboard(mDropdown);
        ViewGroup parent = (ViewGroup) mDropdown.getParent();
        if (parent != null) parent.requestChildFocus(mDropdown, mDropdown);
        mDropdown.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
    }
}
