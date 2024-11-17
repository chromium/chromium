// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.FrameLayout;
import android.widget.TextView.OnEditorActionListener;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.List;

/** Handles validation and display of one field from the {@link EditorProperties.ItemType}. */
// TODO(b/173103628): Re-enable this
// @VisibleForTesting
class TextFieldView extends FrameLayout implements FieldView {
    // TODO(crbug.com/40824084): Replace with EditorDialog field once migrated.
    /** The indicator for input fields that are required. */
    public static final String REQUIRED_FIELD_INDICATOR = "*";

    @Nullable private static EditorObserverForTest sObserverForTest;

    @Nullable private Runnable mDoneRunnable;

    @SuppressWarnings("WrongConstant") // https://crbug.com/1038784
    private final OnEditorActionListener mEditorActionListener =
            (view, actionId, event) -> {
                if (actionId == EditorInfo.IME_ACTION_DONE && mDoneRunnable != null) {
                    mDoneRunnable.run();
                    return true;
                } else if (actionId != EditorInfo.IME_ACTION_NEXT) {
                    return false;
                }
                View next = view.focusSearch(View.FOCUS_FORWARD);
                if (next == null) {
                    return false;
                }
                next.requestFocus();
                return true;
            };

    private PropertyModel mEditorFieldModel;
    private TextInputLayout mInputLayout;
    private AutoCompleteTextView mInput;
    private View mIconsLayer;
    private boolean mShowRequiredIndicator;
    @Nullable private EditorFieldValidator mValidator;
    @Nullable private TextWatcher mTextFormatter;
    private boolean mInFocusChange;
    private boolean mInValueChange;

    public TextFieldView(Context context, final PropertyModel fieldModel) {
        super(context);
        mEditorFieldModel = fieldModel;

        LayoutInflater.from(context).inflate(R.layout.payments_request_editor_textview, this, true);
        mInputLayout = (TextInputLayout) findViewById(R.id.text_input_layout);

        mInput = (AutoCompleteTextView) mInputLayout.findViewById(R.id.text_view);
        mInput.setOnEditorActionListener(mEditorActionListener);
        // AutoCompleteTextView requires and explicit onKeyListener to show the OSK upon receiving
        // a KEYCODE_DPAD_CENTER.
        mInput.setOnKeyListener(
                (v, keyCode, event) -> {
                    if (!(keyCode == KeyEvent.KEYCODE_DPAD_CENTER
                            && event.getAction() == KeyEvent.ACTION_UP)) {
                        return false;
                    }
                    InputMethodManager imm =
                            (InputMethodManager)
                                    v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                    imm.viewClicked(v);
                    imm.showSoftInput(v, 0);
                    return true;
                });

        setShowRequiredIndicator(/* showRequiredIndicator= */ false);

        mIconsLayer = findViewById(R.id.icons_layer);
        mIconsLayer.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // Padding at the end of mInput to preserve space for mIconsLayer.
                        mInput.setPaddingRelative(
                                ViewCompat.getPaddingStart(mInput),
                                mInput.getPaddingTop(),
                                mIconsLayer.getWidth(),
                                mInput.getPaddingBottom());
                    }
                });

        mInput.setOnFocusChangeListener(
                new OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        mInFocusChange = true;
                        mEditorFieldModel.set(FOCUSED, hasFocus);
                        mInFocusChange = false;

                        if (!hasFocus && mValidator != null) {
                            // Validate the field when the user de-focuses it.
                            // We do not validate the form initially when all of the fields are
                            // empty to avoid showing error messages in all of the fields.
                            mValidator.validate(mEditorFieldModel);
                        }
                    }
                });

        // Update the model as the user edits the field.
        mInput.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        fieldModel.set(VALUE, s.toString());
                        if (sObserverForTest != null) {
                            sObserverForTest.onEditorTextUpdate();
                        }
                    }

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        if (mInput.hasFocus() && !mInValueChange) {
                            if (mValidator != null) {
                                mValidator.onUserEditedField();
                            }

                            // Hide the error message and wait till the user finishes editing the
                            // field to re-show the error label.
                            mEditorFieldModel.set(ERROR_MESSAGE, null);
                        }
                    }
                });
    }

    void setLabel(String label, boolean isRequired) {
        // Build up the label. Required fields are indicated by appending a '*'.
        if (isRequired && mShowRequiredIndicator) {
            label += REQUIRED_FIELD_INDICATOR;
        }
        mInputLayout.setHint(label);
        mInput.setContentDescription(label);
    }

    void setValidator(@Nullable EditorFieldValidator validator) {
        mValidator = validator;
    }

    void setErrorMessage(@Nullable String errorMessage) {
        mInputLayout.setError(errorMessage);
        if (sObserverForTest != null && errorMessage != null) {
            sObserverForTest.onEditorValidationError();
        }
    }

    void setValue(@Nullable String value) {
        value = value == null ? "" : value;
        if (mInput.getText().toString().equals(value)) {
            return;
        }
        // {@link mTextFormatter#afterTextChanged()} can trigger a nested {@link setValue()}
        // call.
        boolean inNestedValueChange = mInValueChange;
        mInValueChange = true;
        mInput.setText(value);
        if (mTextFormatter != null) {
            mTextFormatter.afterTextChanged(mInput.getText());
        }
        mInValueChange = inNestedValueChange;
    }

    void setTextInputType(int textInputType) {
        mInput.setInputType(textInputType);
    }

    void setTextSuggestions(@Nullable List<String> suggestions) {
        // Display any autofill suggestions.
        if (suggestions != null && !suggestions.isEmpty()) {
            mInput.setAdapter(
                    new ArrayAdapter<>(
                            getContext(),
                            android.R.layout.simple_spinner_dropdown_item,
                            suggestions));
            mInput.setThreshold(0);
        }
    }

    void setTextFormatter(@Nullable TextWatcher formatter) {
        mTextFormatter = formatter;
        if (mTextFormatter != null) {
            mInput.addTextChangedListener(mTextFormatter);
            mTextFormatter.afterTextChanged(mInput.getText());
        }
    }

    void setDoneRunnable(@Nullable Runnable doneRunnable) {
        mDoneRunnable = doneRunnable;
    }

    @Override
    public void setShowRequiredIndicator(boolean showRequiredIndicator) {
        mShowRequiredIndicator = showRequiredIndicator;
        setLabel(mEditorFieldModel.get(LABEL), mEditorFieldModel.get(IS_REQUIRED));
    }

    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        if (changed) {
            // Align the bottom of mIconsLayer to the bottom of mInput (mIconsLayer overlaps
            // mInput).
            // Note one:   mIconsLayer can not be put inside mInputLayout to display on top of
            // mInput since mInputLayout is LinearLayout in essential.
            // Note two:   mIconsLayer and mInput can not be put in ViewGroup to display over each
            // other inside mInputLayout since mInputLayout must contain an instance of EditText
            // child view.
            // Note three: mInputLayout's bottom changes when displaying error.
            float offset =
                    mInputLayout.getY()
                            + mInput.getY()
                            + (float) mInput.getHeight()
                            - (float) mIconsLayer.getHeight()
                            - mIconsLayer.getTop();
            mIconsLayer.setTranslationY(offset);
        }
    }

    /**
     * @return The AutoCompleteTextView this field associates
     */
    public AutoCompleteTextView getEditText() {
        return mInput;
    }

    public TextInputLayout getInputLayoutForTesting() {
        return mInputLayout;
    }

    @Override
    public boolean validate() {
        if (mValidator != null) {
            mValidator.validate(mEditorFieldModel);
        }
        return mInputLayout.getError() == null;
    }

    @Override
    public boolean isRequired() {
        return mEditorFieldModel.get(IS_REQUIRED);
    }

    @Override
    public void scrollToAndFocus() {
        if (mInFocusChange) return;

        ViewGroup parent = (ViewGroup) getParent();
        if (parent != null) parent.requestChildFocus(this, this);
        requestFocus();
        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    public void removeTextChangedListeners() {
        if (mTextFormatter != null) {
            mInput.removeTextChangedListener(mTextFormatter);
        }
    }

    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
    }
}
