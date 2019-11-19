// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.prefeditor;

import android.content.Context;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
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

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.ChromeTextInputLayout;

/** Handles validation and display of one field from the {@link EditorFieldModel}. */
@VisibleForTesting
public class EditorTextField extends FrameLayout implements EditorFieldView, View.OnClickListener {
    private EditorFieldModel mEditorFieldModel;
    private OnEditorActionListener mEditorActionListener;
    private ChromeTextInputLayout mInputLayout;
    private AutoCompleteTextView mInput;
    private View mIconsLayer;
    private ImageView mActionIcon;
    private ImageView mValueIcon;
    private int mValueIconId;
    private boolean mHasFocusedAtLeastOnce;
    @Nullable
    private EditorObserverForTest mObserverForTest;

    public EditorTextField(Context context, final EditorFieldModel fieldModel,
            OnEditorActionListener actionListener, @Nullable InputFilter filter,
            @Nullable TextWatcher formatter, @Nullable EditorObserverForTest observer) {
        super(context);
        assert fieldModel.getInputTypeHint() != EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN;
        mEditorFieldModel = fieldModel;
        mEditorActionListener = actionListener;
        mObserverForTest = observer;

        LayoutInflater.from(context).inflate(R.layout.payments_request_editor_textview, this, true);
        mInputLayout = (ChromeTextInputLayout) findViewById(R.id.text_input_layout);

        // Build up the label.  Required fields are indicated by appending a '*'.
        CharSequence label = fieldModel.getLabel();
        if (fieldModel.isRequired()) label = label + EditorDialog.REQUIRED_FIELD_INDICATOR;
        mInputLayout.setHint(label);

        mInput = (AutoCompleteTextView) mInputLayout.findViewById(R.id.text_view);
        mInput.setText(fieldModel.getValue());
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

        if (fieldModel.getActionIconAction() != null) {
            mActionIcon = (ImageView) mIconsLayer.findViewById(R.id.action_icon);
            mActionIcon.setImageDrawable(TintedDrawable.constructTintedDrawable(context,
                    fieldModel.getActionIconResourceId(), R.color.default_icon_color_blue));
            mActionIcon.setContentDescription(context.getResources().getString(
                    fieldModel.getActionIconDescriptionForAccessibility()));
            mActionIcon.setOnClickListener(this);
            mActionIcon.setVisibility(VISIBLE);
        }

        if (fieldModel.getValueIconGenerator() != null) {
            mValueIcon = (ImageView) mIconsLayer.findViewById(R.id.value_icon);
            mValueIcon.setVisibility(VISIBLE);
        }

        // Validate the field when the user de-focuses it.
        mInputLayout.addEditTextOnFocusChangeListener((v, hasFocus) -> {
            if (hasFocus) {
                mHasFocusedAtLeastOnce = true;
            } else if (mHasFocusedAtLeastOnce) {
                // Show no errors until the user has already tried to edit the field once.
                updateDisplayedError(!mEditorFieldModel.isValid());
            }
        });

        // Update the model as the user edits the field.
        mInput.addTextChangedListener(new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
                fieldModel.setValue(s.toString());
                updateDisplayedError(false);
                updateFieldValueIcon(false);
                if (mObserverForTest != null) {
                    mObserverForTest.onEditorTextUpdate();
                }
                if (!mEditorFieldModel.isLengthMaximum()) return;
                updateDisplayedError(true);
                if (isValid()) {
                    // Simulate editor action to select next selectable field.
                    mEditorActionListener.onEditorAction(mInput, EditorInfo.IME_ACTION_NEXT,
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
                }
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (mInput.hasFocus()) {
                    fieldModel.setCustomErrorMessage(null);
                }
            }
        });

        // Display any autofill suggestions.
        if (fieldModel.getSuggestions() != null && !fieldModel.getSuggestions().isEmpty()) {
            mInput.setAdapter(new ArrayAdapter<CharSequence>(getContext(),
                    android.R.layout.simple_spinner_dropdown_item, fieldModel.getSuggestions()));
            mInput.setThreshold(0);
        }

        if (filter != null) mInput.setFilters(new InputFilter[] {filter});
        if (formatter != null) {
            mInput.addTextChangedListener(formatter);
            formatter.afterTextChanged(mInput.getText());
        }

        switch (fieldModel.getInputTypeHint()) {
            case EditorFieldModel.INPUT_TYPE_HINT_CREDIT_CARD:
            // Intentionally fall through.
            //
            // There's no keyboard that allows numbers, spaces, and "-" only, so use the phone
            // keyboard instead. The phone keyboard has more symbols than necessary. A filter
            // should be used to prevent input of phone number symbols that are not relevant for
            // credit card numbers, e.g., "+", "*", and "#".
            //
            // The number keyboard is not suitable, because it filters out everything except
            // digits.
            case EditorFieldModel.INPUT_TYPE_HINT_PHONE:
                // Show the keyboard with numbers and phone-related symbols.
                mInput.setInputType(InputType.TYPE_CLASS_PHONE);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_EMAIL:
                mInput.setInputType(
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES:
                // TODO(rouslan): Provide a hint to the keyboard that the street lines are
                // likely to have numbers.
                mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME:
                mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_PERSON_NAME);
                break;
            case EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC:
            // Intentionally fall through.
            // TODO(rouslan): Provide a hint to the keyboard that postal code and sorting
            // code are likely to have numbers.
            case EditorFieldModel.INPUT_TYPE_HINT_REGION:
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

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        if (hasWindowFocus) updateFieldValueIcon(true);
    }

    @Override
    public void onClick(View v) {
        mEditorFieldModel.getActionIconAction().run();
    }

    /** @return The EditorFieldModel that the TextView represents. */
    public EditorFieldModel getFieldModel() {
        return mEditorFieldModel;
    }

    /** @return The AutoCompleteTextView this field associates*/
    public AutoCompleteTextView getEditText() {
        return mInput;
    }

    @Override
    public boolean isValid() {
        return mEditorFieldModel.isValid();
    }

    @Override
    public void updateDisplayedError(boolean showError) {
        mInputLayout.setError(showError ? mEditorFieldModel.getErrorMessage() : null);
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
        mInput.setText(mEditorFieldModel.getValue());
    }

    private void updateFieldValueIcon(boolean force) {
        if (mValueIcon == null) return;

        int iconId = mEditorFieldModel.getValueIconGenerator().getIconResourceId(mInput.getText());
        if (mValueIconId == iconId && !force) return;
        mValueIconId = iconId;
        if (mValueIconId == 0) {
            mValueIcon.setVisibility(GONE);
        } else {
            mValueIcon.setImageDrawable(AppCompatResources.getDrawable(getContext(), mValueIconId));
            mValueIcon.setVisibility(VISIBLE);
        }
    }
}
