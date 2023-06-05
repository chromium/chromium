// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorFieldValidator;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Representation of a single input text field in an editor. Can be used, for example, for a phone
 * input field.
 */
public class EditorFieldModel {
    /* Indicates that the length counter is disabled. */
    public static final int LENGTH_COUNTER_LIMIT_NONE = 0;

    private final @ItemType int mFieldType;
    private final @TextInputType int mTextInputType;

    @Nullable
    private List<DropdownKeyValue> mDropdownKeyValues;
    @Nullable
    private HashMap<String, String> mDropdownKeyToValueMap;
    @Nullable
    private HashMap<String, String> mDropdownValueToKeyMap;
    @Nullable
    private Set<String> mDropdownKeys;
    @Nullable
    private List<String> mSuggestions;
    @Nullable
    private TextWatcher mFormatter;
    @Nullable
    private EditorFieldValidator mValidator;
    @Nullable
    private String mRequiredErrorMessage;
    @Nullable
    private String mInvalidErrorMessage;
    @Nullable
    private String mCustomErrorMessage;
    @Nullable
    private String mErrorMessage;
    @Nullable
    private String mLabel;
    @Nullable
    private String mValue;
    @Nullable
    private String mHint;
    @Nullable
    private Callback<Pair<String, Runnable>> mDropdownCallback;
    private boolean mIsFullLine = true;
    private int mLengthCounterLimit = LENGTH_COUNTER_LIMIT_NONE;

    /**
     * Constructs a dropdown field model.
     *
     * @param label             The human-readable label for user to understand the type of data
     *                          that should be entered into this field.
     * @param dropdownKeyValues The keyed values to display in the dropdown.
     * @param hint              The optional hint to be displayed when no value is selected.
     */
    public static EditorFieldModel createDropdown(@Nullable String label,
            List<DropdownKeyValue> dropdownKeyValues, @Nullable String hint) {
        assert dropdownKeyValues != null;
        EditorFieldModel result = new EditorFieldModel(ItemType.DROPDOWN);
        result.mLabel = label;
        result.mHint = hint;
        result.setDropdownKeyValues(dropdownKeyValues);
        return result;
    }

    /**
     * Constructs a dropdown field model with a validator.
     *
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param dropdownKeyValues    The keyed values to display in the dropdown.
     * @param validator            The validator for the values in this field.
     * @param requiredErrorMessage The error message that indicates to the user that they
     *                             cannot leave this field empty.
     */
    public static EditorFieldModel createDropdown(@Nullable String label,
            List<DropdownKeyValue> dropdownKeyValues, EditorFieldValidator validator,
            String invalidErrorMessage) {
        assert dropdownKeyValues != null;
        assert validator != null;
        assert invalidErrorMessage != null;
        EditorFieldModel result = createDropdown(label, dropdownKeyValues, null /* hint */);
        result.mValidator = validator;
        result.mInvalidErrorMessage = invalidErrorMessage;
        return result;
    }

    /** Constructs a text input field model without any special text formatting hints. */
    public static EditorFieldModel createTextInput() {
        return createTextInput(TextInputType.PLAIN_TEXT_INPUT);
    }

    /**
     * Constructs a text input field model.
     *
     * @param textInputType The type of text field to create.
     */
    public static EditorFieldModel createTextInput(@TextInputType int textInputType) {
        EditorFieldModel result = new EditorFieldModel(ItemType.TEXT_INPUT, textInputType);
        assert result.isTextField();
        return result;
    }

    /**
     * Constructs a text input field model.
     *
     * @param textInputType        The type of text field to create.
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param suggestions          Optional set of values to suggest to the user.
     * @param formatter            Optional formatter for the values in this field.
     * @param validator            Optional validator for the values in this field.
     * @param valueIconGenerator   Optional icon generator for the values in this field.
     * @param requiredErrorMessage The optional error message that indicates to the user that they
     *                             cannot leave this field empty.
     * @param invalidErrorMessage  The optional error message that indicates to the user that the
     *                             value they have entered is not valid.
     * @param lengthCounterLimit   The maximum number of characters to be allowed by the length
     *                             counter ("cur/max") shown below the focused field. Use
     *                             {@link LENGTH_COUNTER_LIMIT_NONE} to disable the counter.
     * @param value                Optional initial value of this field.
     */
    public static EditorFieldModel createTextInput(@TextInputType int textInputType, String label,
            @Nullable Set<String> suggestions, @Nullable TextWatcher formatter,
            @Nullable EditorFieldValidator validator, @Nullable String requiredErrorMessage,
            @Nullable String invalidErrorMessage, int lengthCounterLimit, @Nullable String value) {
        assert label != null;
        EditorFieldModel result = new EditorFieldModel(ItemType.TEXT_INPUT, textInputType);
        assert result.isTextField();
        result.mSuggestions = suggestions == null ? null : new ArrayList<String>(suggestions);
        result.mFormatter = formatter;
        result.mValidator = validator;
        result.mInvalidErrorMessage = invalidErrorMessage;
        result.mRequiredErrorMessage = requiredErrorMessage;
        result.mLabel = label;
        result.mValue = value;
        result.mLengthCounterLimit = Math.max(lengthCounterLimit, LENGTH_COUNTER_LIMIT_NONE);
        return result;
    }

    public EditorFieldModel(@ItemType int fieldType) {
        assert fieldType != ItemType.TEXT_INPUT;
        mFieldType = fieldType;
        // This value is meaningless when field type is not text input.
        mTextInputType = TextInputType.PLAIN_TEXT_INPUT;
    }

    public EditorFieldModel(@ItemType int fieldType, @TextInputType int textInputType) {
        assert fieldType == ItemType.TEXT_INPUT;
        mFieldType = fieldType;
        mTextInputType = textInputType;
    }

    /** @return The value formatter or null if not exist. */
    @Nullable
    public TextWatcher getFormatter() {
        assert isTextField();
        return mFormatter;
    }

    /** @return Whether the input is a text field. */
    public boolean isTextField() {
        return mFieldType == ItemType.TEXT_INPUT;
    }

    /** @return Whether the input is a dropdown field. */
    public boolean isDropdownField() {
        return mFieldType == ItemType.DROPDOWN;
    }

    /** @return The type of this field, for example, {@link ItemType.DROPDOWN}. */
    public @ItemType int getFieldType() {
        return mFieldType;
    }

    /** @return The type of this text input field. */
    public @TextInputType int getTextInputType() {
        assert isTextField();
        return mTextInputType;
    }

    /** @return The dropdown key-value pairs. */
    public List<DropdownKeyValue> getDropdownKeyValues() {
        assert isDropdownField();
        return mDropdownKeyValues;
    }

    /** @return The dropdown keys. */
    public Set<String> getDropdownKeys() {
        assert isDropdownField();
        return mDropdownKeys;
    }

    /**
     * In the dropdown list, finds the key that corresponds to the value.
     * If the value is not in the list, returns null.
     * This assumes a one-to-one relation between keys and values.
     *
     * @param value The value to lookup.
     * @return The key that corresponds to the value.
     */
    @Nullable
    public String getDropdownKeyByValue(@Nullable String value) {
        assert isDropdownField();
        if (value == null) {
            return null;
        }
        return mDropdownValueToKeyMap.get(value);
    }

    /**
     * In the dropdown list, finds the value that corresponds to the key.
     * If the key is not in the list, returns null.
     * This assumes a one-to-one relation between keys and values.
     *
     * @param key The key to lookup.
     * @return The value that corresponds to the key.
     */
    @Nullable
    public String getDropdownValueByKey(@Nullable String key) {
        assert isDropdownField();
        return mDropdownKeyToValueMap.get(key);
    }

    /** Updates the dropdown key values. */
    public void setDropdownKeyValues(List<DropdownKeyValue> dropdownKeyValues) {
        assert isDropdownField();
        mDropdownKeyValues = dropdownKeyValues;
        mDropdownKeys = new HashSet<>();
        mDropdownKeyToValueMap = new HashMap<>();
        mDropdownValueToKeyMap = new HashMap<>();
        for (int i = 0; i < mDropdownKeyValues.size(); i++) {
            mDropdownKeys.add(mDropdownKeyValues.get(i).getKey());
            mDropdownValueToKeyMap.put(
                    mDropdownKeyValues.get(i).getValue(), mDropdownKeyValues.get(i).getKey());
            mDropdownKeyToValueMap.put(
                    mDropdownKeyValues.get(i).getKey(), mDropdownKeyValues.get(i).getValue());
        }
        assert mDropdownKeyValues.size() == mDropdownKeys.size();
    }

    /** @return The human-readable label for this field. */
    public String getLabel() {
        return mLabel;
    }

    /** @return The human-readable hint for this dropdown field. */
    public String getHint() {
        assert isDropdownField();
        return mHint;
    }

    /**
     * Updates the label.
     *
     * @param label The new label to use.
     */
    public void setLabel(String label) {
        mLabel = label;
    }

    /** @return Suggested values for this field. Can be null. */
    @Nullable
    public List<String> getSuggestions() {
        return mSuggestions;
    }

    /** @return The error message for the last validation. Can be null if no error was reported. */
    @Nullable
    public String getErrorMessage() {
        return mErrorMessage;
    }

    /** Updates the custom error message */
    public void setCustomErrorMessage(@Nullable String errorMessage) {
        mCustomErrorMessage = errorMessage;
    }

    /**
     * @return The value that the user has typed into the field or the key of the value that the
     *          user has selected in the dropdown. Can be null.
     */
    @Nullable
    public String getValue() {
        return mValue;
    }

    /**
     * Updates the value of this field. Does not trigger validation or update the last error
     * message. Can be called on a dropdown to initialize it, but will not fire the dropdown
     * callback.
     *
     * @param value The new value that the user has typed in or the initial key for the dropdown.
     */
    public void setValue(@Nullable String userTypedValueOrInitialDropdownKey) {
        mValue = userTypedValueOrInitialDropdownKey;
    }

    /**
     * Updates the dropdown selection and fires the dropdown callback.
     *
     * @param key      The new dropdown key.
     * @param callback The callback to invoke when the change has been processed.
     */
    public void setDropdownKey(@Nullable String key, Runnable callback) {
        assert isDropdownField();
        // The mValue can only be set to null if there is a hint.
        if (key == null && mHint == null) {
            return;
        }
        mValue = key;
        if (mDropdownCallback != null) {
            mDropdownCallback.onResult(new Pair<String, Runnable>(key, callback));
        }
    }

    /** @return Whether or not the field is required. */
    public boolean isRequired() {
        return !TextUtils.isEmpty(mRequiredErrorMessage);
    }

    /**
     * Updates the required error message.
     *
     * @param message The error message to use if this field is required, but empty. If null, then
     *                this field is optional.
     */
    public void setRequiredErrorMessage(@Nullable String message) {
        mRequiredErrorMessage = message;
    }

    /**
     * Returns true if the field value is valid. Also updates the error message.
     *
     * @return Whether the field value is valid.
     */
    public boolean isValid() {
        if (!TextUtils.isEmpty(mCustomErrorMessage)) {
            mErrorMessage = mCustomErrorMessage;
            return false;
        }

        if (isRequired()
                && (TextUtils.isEmpty(mValue) || TextUtils.getTrimmedLength(mValue) == 0)) {
            mErrorMessage = mRequiredErrorMessage;
            return false;
        }

        if (mValidator != null && !mValidator.isValid(mValue)) {
            mErrorMessage = mInvalidErrorMessage;
            return false;
        }

        mErrorMessage = null;
        return true;
    }

    /**
     * Returns true if the field value length is maximum among all the possible valid values in this
     * field.
     *
     * @Return Whether the field value length is maximum.
     */
    public boolean isLengthMaximum() {
        return mValidator == null ? false : mValidator.isLengthMaximum(mValue);
    }

    /**
     * Sets the dropdown callback.
     *
     * @param callback The callback to invoke when the dropdown selection has changed. The first
     *                 element in the callback pair is the dropdown key. The second element is the
     *                 callback to invoke after the dropdown change has been processed.
     */
    public void setDropdownCallback(Callback<Pair<String, Runnable>> callback) {
        assert isDropdownField();
        mDropdownCallback = callback;
    }

    /**
     * @return True if the input field should take up the full line, instead of sharing with other
     *          input fields. This is the default.
     */
    public boolean isFullLine() {
        return mIsFullLine;
    }

    /**
     * Sets whether this input field should take up the full line. All fields take up the full line
     * by default.
     *
     * @param isFullLine Whether the input field should take up the full line.
     */
    public void setIsFullLine(boolean isFullLine) {
        mIsFullLine = isFullLine;
    }

    /**
     * @return The maximum number of characters allowed by the length counter ("cur/max") shown
     * below the focused field. If the value equals {@link LENGTH_COUNTER_LIMIT_NONE}, the counter
     * is not shown, which is the default behavior.
     */
    public int getLengthCounterLimit() {
        return mLengthCounterLimit;
    }

    /**
     * Sets the maximum number of characters to be allowed by the length counter ("cur/max") shown
     * below the focused field. If the value equals {@link LENGTH_COUNTER_LIMIT_NONE} or any
     * nonpositive value, the counter won't be shown, which is the default behavior.
     *
     * @param lengthCounterLimit The maximum number of characters for the length counter.
     */
    public void setLengthCounterLimit(int lengthCounterLimit) {
        mLengthCounterLimit = Math.max(lengthCounterLimit, LENGTH_COUNTER_LIMIT_NONE);
    }

    /**
     * @return whether the length counter should be shown below the focused field.
     */
    public boolean hasLengthCounter() {
        return mLengthCounterLimit != EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE;
    }
}
