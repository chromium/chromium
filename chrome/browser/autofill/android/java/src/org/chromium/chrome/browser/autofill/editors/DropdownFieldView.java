// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
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
import android.widget.AutoCompleteTextView;
import android.widget.Spinner;
import android.widget.TextView;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Helper class for creating a dropdown view with a label. */
@NullMarked
class DropdownFieldView implements FieldView {
    private @Nullable static EditorObserverForTest sObserverForTest;

    private final Context mContext;
    private final PropertyModel mFieldModel;
    private final View mLayout;
    // Fields are conditionally initialized based on `isContained()`. `mDropdown` is used when
    // `isContained()` is true.
    private final @Nullable TextView mLabel;
    private final @Nullable Spinner mLegacyDropdown;
    private final @Nullable View mUnderline;
    private final @Nullable AutoCompleteTextView mDropdown;

    private final TextView mErrorLabel;
    private int mSelectedIndex;
    private @Nullable ArrayAdapter<String> mAdapter;
    private @Nullable String mHint;
    private @Nullable EditorFieldValidator mValidator;

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
                        .inflate(R.layout.autofill_editor_dialog_dropdown, root, false);

        mErrorLabel = mLayout.findViewById(R.id.spinner_error);

        if (isContained()) {
            mLayout.findViewById(R.id.legacy_dropdown_container).setVisibility(View.GONE);
            mLayout.findViewById(R.id.dropdown_container).setVisibility(View.VISIBLE);
            mDropdown = mLayout.findViewById(R.id.dropdown_outlined);
            mLegacyDropdown = null;
            mLabel = null;
            mUnderline = null;
            mDropdown.setTag(this);
            mDropdown.setOnItemClickListener(
                    (parent, view, position, id) -> handleItemSelected(position));
            mDropdown.setOnClickListener(v -> ((AutoCompleteTextView) v).showDropDown());
            mDropdown.setOnFocusChangeListener(
                    (v, hasFocus) -> {
                        if (hasFocus) {
                            KeyboardVisibilityDelegate.getInstance().hideKeyboard(v);
                        }
                    });
        } else {
            mLabel = mLayout.findViewById(R.id.spinner_label);
            mUnderline = mLayout.findViewById(R.id.spinner_underline);
            mLegacyDropdown = mLayout.findViewById(R.id.spinner);
            mDropdown = null;
            mLegacyDropdown.setTag(this);
            mLegacyDropdown.setOnItemSelectedListener(
                    new OnItemSelectedListener() {
                        @Override
                        public void onItemSelected(
                                AdapterView<?> parent, View view, int position, long id) {
                            handleItemSelected(position);
                        }

                        @Override
                        public void onNothingSelected(AdapterView<?> parent) {}
                    });
            mLegacyDropdown.setOnTouchListener(
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
    }

    void setLabel(String label, boolean isRequired) {
        label = isRequired ? label + EditorDialogView.REQUIRED_FIELD_INDICATOR : label;
        if (isContained()) {
            ((TextInputLayout) mLayout.findViewById(R.id.dropdown_container)).setHint(label);
        } else {
            assumeNonNull(mLabel).setText(label);
        }
    }

    void setDropdownValues(List<String> values, @Nullable String hint) {
        mHint = hint;
        if (isContained()) {
            mAdapter =
                    new ArrayAdapter<>(
                            mContext, android.R.layout.simple_dropdown_item_1line, values);
            assumeNonNull(mDropdown).setAdapter(mAdapter);
        } else {
            if (mHint != null) {
                mAdapter =
                        new HintedDropDownAdapter<>(
                                mContext,
                                R.layout.multiline_spinner_item,
                                R.id.spinner_item,
                                values,
                                mHint);
                // Wrap the TextView in the dropdown popup around with a FrameLayout to display the
                // text in multiple lines.
                // Note that the TextView in the dropdown popup is displayed in a DropDownListView
                // for the dropdown style Spinner and the DropDownListView sets to display TextView
                // instance in a single line.
                mAdapter.setDropDownViewResource(R.layout.payment_request_dropdown_item);
            } else {
                mAdapter =
                        new DropdownFieldAdapter<>(
                                mContext, R.layout.multiline_spinner_item, values);
                mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            }
            assumeNonNull(mLegacyDropdown).setAdapter(mAdapter);
        }
    }

    void setValue(@Nullable String value) {
        if (mAdapter == null || mAdapter.isEmpty()) {
            // Can't set value when adapter hasn't been initialized or is empty.
            mSelectedIndex = 0;
            if (isContained()) {
                CharSequence hint =
                        ((TextInputLayout) mLayout.findViewById(R.id.dropdown_container)).getHint();
                assumeNonNull(mDropdown).setContentDescription(hint);
            } else {
                assumeNonNull(mLegacyDropdown)
                        .setContentDescription(mLabel != null ? mLabel.getText() : "");
            }
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

        if (isContained()) {
            assumeNonNull(mDropdown).setText(mAdapter.getItem(mSelectedIndex), false);
            CharSequence hint =
                    ((TextInputLayout) mLayout.findViewById(R.id.dropdown_container)).getHint();
            assumeNonNull(mDropdown)
                    .setContentDescription(
                            hint
                                    + "/"
                                    + assumeNonNull(mAdapter.getItem(mSelectedIndex)).toString());
        } else {
            assumeNonNull(mLegacyDropdown).setSelection(mSelectedIndex);
            assumeNonNull(mLegacyDropdown)
                    .setContentDescription(
                            (mLabel != null ? mLabel.getText() : "")
                                    + "/"
                                    + assumeNonNull(mAdapter.getItem(mSelectedIndex)).toString());
        }
    }

    void setErrorMessage(@Nullable String errorMessage) {
        if (isContained()) {
            ((TextInputLayout) mLayout.findViewById(R.id.dropdown_container))
                    .setError(errorMessage);
        } else {
            View view = assumeNonNull(mLegacyDropdown).getSelectedView();
            if (errorMessage == null) {
                // {@link Spinner#getSelectedView()} is null in JUnit tests.
                if (view != null && view instanceof TextView) {
                    ((TextView) view).setError(null);
                }
                assumeNonNull(mUnderline)
                        .setBackgroundColor(mContext.getColor(R.color.baseline_neutral_40));
                mErrorLabel.setText(null);
                mErrorLabel.setVisibility(View.GONE);
                return;
            }

            Drawable drawable =
                    TraceEventVectorDrawableCompat.create(
                            mContext.getResources(), R.drawable.ic_error, mContext.getTheme());
            assumeNonNull(drawable);
            drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
            if (view != null && view instanceof TextView) {
                ((TextView) view).setError(errorMessage, drawable);
            }
            assumeNonNull(mUnderline)
                    .setBackgroundColor(mContext.getColor(R.color.default_text_color_error));
            mErrorLabel.setText(errorMessage);
            mErrorLabel.setVisibility(View.VISIBLE);
        }

        if (errorMessage != null && sObserverForTest != null) {
            sObserverForTest.onEditorValidationError();
        }
    }

    void setValidator(@Nullable EditorFieldValidator validator) {
        mValidator = validator;
    }

    /**
     * @return The View containing everything.
     */
    public View getLayout() {
        return mLayout;
    }

    /** @return The PropertyModel that the DropdownFieldView represents. */
    public PropertyModel getFieldModel() {
        return mFieldModel;
    }

    /**
     * @return The label view for the spinner.
     */
    public @Nullable View getLabel() {
        return mLabel;
    }

    /**
     * @return The dropdown view itself.
     */
    public @Nullable View getDropdown() {
        if (isContained()) {
            return mDropdown;
        } else {
            return mLegacyDropdown;
        }
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

        View dropdown = getDropdown();
        if (dropdown == null) {
            return;
        }
        KeyboardVisibilityDelegate.getInstance().hideKeyboard(dropdown);
        ViewGroup parent = (ViewGroup) dropdown.getParent();
        if (parent != null) parent.requestChildFocus(dropdown, dropdown);
        dropdown.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    private void handleItemSelected(int position) {
        if (mSelectedIndex != position) {
            assumeNonNull(mAdapter);
            String key =
                    getDropdownKeyByValue(mFieldModel, assumeNonNull(mAdapter.getItem(position)));
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

    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    private static boolean isContained() {
        return ChromeFeatureList.sAndroidSettingsContainment.isEnabled();
    }
}
