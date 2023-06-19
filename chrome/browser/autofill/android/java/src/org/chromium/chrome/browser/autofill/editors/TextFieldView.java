// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.CUSTOM_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.LENGTH_COUNTER_LIMIT_NONE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_INPUT_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_LENGTH_COUNTER_LIMIT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_SUGGESTIONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.ALPHA_NUMERIC_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.EMAIL_ADDRESS_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PERSON_NAME_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PHONE_NUMBER_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.REGION_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.STREET_ADDRESS_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.getValidationErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.hasMaximumLength;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.isFieldValid;

import android.content.Context;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
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
import android.widget.ImageView;
import android.widget.TextView.OnEditorActionListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

/** Handles validation and display of one field from the {@link EditorProperties.ItemType}. */
// TODO(b/173103628): Re-enable this
//@VisibleForTesting
class TextFieldView extends FrameLayout implements FieldView {
    // TODO(crbug.com/1300201): Replace with EditorDialog field once migrated.
    /** The indicator for input fields that are required. */
    public static final String REQUIRED_FIELD_INDICATOR = "*";

    @Nullable
    private static EditorObserverForTest sObserverForTest;

    @Nullable
    private final TextWatcher mFormatter;

    private PropertyModel mEditorFieldModel;
    private OnEditorActionListener mEditorActionListener;
    private TextInputLayout mInputLayout;
    private AutoCompleteTextView mInput;
    private View mIconsLayer;
    private ImageView mActionIcon;

    public TextFieldView(Context context, final PropertyModel fieldModel,
            OnEditorActionListener actionListener, @Nullable TextWatcher formatter,
            boolean hasRequiredIndicator) {
        super(context);
        mEditorFieldModel = fieldModel;
        mEditorActionListener = actionListener;

        LayoutInflater.from(context).inflate(R.layout.payments_request_editor_textview, this, true);
        mInputLayout = (TextInputLayout) findViewById(R.id.text_input_layout);

        // Build up the label.  Required fields are indicated by appending a '*'.
        CharSequence label = fieldModel.get(LABEL);
        if (fieldModel.get(IS_REQUIRED) && hasRequiredIndicator) {
            label = label + REQUIRED_FIELD_INDICATOR;
        }
        mInputLayout.setHint(label);

        mInput = (AutoCompleteTextView) mInputLayout.findViewById(R.id.text_view);
        mInput.setText(fieldModel.get(VALUE));
        mInput.setContentDescription(label);
        mInput.setOnEditorActionListener(mEditorActionListener);
        // AutoCompleteTextView requires and explicit onKeyListener to show the OSK upon receiving
        // a KEYCODE_DPAD_CENTER.
        mInput.setOnKeyListener((v, keyCode, event) -> {
            if (!(keyCode == KeyEvent.KEYCODE_DPAD_CENTER
                        && event.getAction() == KeyEvent.ACTION_UP)) {
                return false;
            }
            InputMethodManager imm = (InputMethodManager) v.getContext().getSystemService(
                    Context.INPUT_METHOD_SERVICE);
            imm.viewClicked(v);
            imm.showSoftInput(v, 0);
            return true;
        });

        mIconsLayer = findViewById(R.id.icons_layer);
        mIconsLayer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Padding at the end of mInput to preserve space for mIconsLayer.
                ViewCompat.setPaddingRelative(mInput, ViewCompat.getPaddingStart(mInput),
                        mInput.getPaddingTop(), mIconsLayer.getWidth(), mInput.getPaddingBottom());
            }
        });

        mInput.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (!hasFocus) {
                    // Validate the field when the user de-focuses it.
                    // Show no errors until the user has already tried to edit the field once.
                    updateDisplayedError(!isFieldValid(mEditorFieldModel));
                }

                if (mEditorFieldModel.get(TEXT_LENGTH_COUNTER_LIMIT) != LENGTH_COUNTER_LIMIT_NONE) {
                    mInputLayout.setCounterEnabled(hasFocus);
                }
            }
        });

        // Update the model as the user edits the field.
        mInput.addTextChangedListener(new EmptyTextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
                fieldModel.set(VALUE, s.toString());
                updateDisplayedError(false);
                if (sObserverForTest != null) {
                    sObserverForTest.onEditorTextUpdate();
                }
                if (!hasMaximumLength(mEditorFieldModel)) return;
                updateDisplayedError(true);
                if (isValid()) {
                    // Simulate editor action to select next selectable field.
                    mEditorActionListener.onEditorAction(mInput, EditorInfo.IME_ACTION_NEXT,
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
                }
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (mInput.hasFocus()) {
                    fieldModel.set(CUSTOM_ERROR_MESSAGE, null);
                }
            }
        });

        // Display any autofill suggestions.
        if (fieldModel.get(TEXT_SUGGESTIONS) != null
                && !fieldModel.get(TEXT_SUGGESTIONS).isEmpty()) {
            mInput.setAdapter(
                    new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_dropdown_item,
                            fieldModel.get(TEXT_SUGGESTIONS)));
            mInput.setThreshold(0);
        }

        final int lengthCounter = mEditorFieldModel.get(TEXT_LENGTH_COUNTER_LIMIT);
        if (lengthCounter != LENGTH_COUNTER_LIMIT_NONE) {
            // Limit input length for field and counter.
            mInput.setFilters(new InputFilter[] {new InputFilter.LengthFilter(lengthCounter)});
            mInputLayout.setCounterMaxLength(lengthCounter);
        }

        mFormatter = formatter;
        if (formatter != null) {
            mInput.addTextChangedListener(formatter);
            formatter.afterTextChanged(mInput.getText());
        }

        switch (fieldModel.get(TEXT_INPUT_TYPE)) {
            case PHONE_NUMBER_INPUT:
                // Show the keyboard with numbers and phone-related symbols.
                mInput.setInputType(InputType.TYPE_CLASS_PHONE);
                break;
            case EMAIL_ADDRESS_INPUT:
                mInput.setInputType(
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
                break;
            case STREET_ADDRESS_INPUT:
                // TODO(rouslan): Provide a hint to the keyboard that the street lines are
                // likely to have numbers.
                mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
            case PERSON_NAME_INPUT:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_PERSON_NAME);
                break;
            case ALPHA_NUMERIC_INPUT:
            // Intentionally fall through.
            // TODO(rouslan): Provide a hint to the keyboard that postal code and sorting
            // code are likely to have numbers.
            case REGION_INPUT:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
            default:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
        }
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
            float offset = mInputLayout.getY() + mInput.getY() + (float) mInput.getHeight()
                    - (float) mIconsLayer.getHeight() - mIconsLayer.getTop();
            mIconsLayer.setTranslationY(offset);
        }
    }

    /** @return The PropertyModel that the TextView represents. */
    public PropertyModel getFieldModel() {
        return mEditorFieldModel;
    }

    /** @return The AutoCompleteTextView this field associates*/
    public AutoCompleteTextView getEditText() {
        return mInput;
    }

    @Override
    public boolean isValid() {
        return isFieldValid(mEditorFieldModel);
    }

    @Override
    public boolean isRequired() {
        return mEditorFieldModel.get(IS_REQUIRED);
    }

    @Override
    public void updateDisplayedError(boolean showError) {
        mInputLayout.setError(showError ? getValidationErrorMessage(mEditorFieldModel) : null);
    }

    @Override
    public void scrollToAndFocus() {
        ViewGroup parent = (ViewGroup) getParent();
        if (parent != null) parent.requestChildFocus(this, this);
        requestFocus();
        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    public void update() {
        mInput.setText(mEditorFieldModel.get(VALUE));
    }

    public void removeTextChangedListeners() {
        if (mFormatter != null) {
            mInput.removeTextChangedListener(mFormatter);
        }
    }

    @VisibleForTesting
    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }
}
