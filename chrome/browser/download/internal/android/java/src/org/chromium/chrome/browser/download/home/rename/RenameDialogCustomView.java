// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.rename;

import static android.content.Context.INPUT_METHOD_SERVICE;

import android.content.Context;
import android.graphics.PorterDuff;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.text.AlertDialogEditText;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.ui.text.EmptyTextWatcher;

/** Content View of dialog in Download Home that allows users to rename a downloaded file. */
public class RenameDialogCustomView extends ScrollView {
    private TextView mErrorMessageView;
    private AlertDialogEditText mFileName;
    private Callback</*Empty*/ Boolean> mEmptyFileNameObserver;

    public RenameDialogCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // ScrollView implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mErrorMessageView = findViewById(R.id.error_message);
        mFileName = findViewById(R.id.file_name);
        mFileName.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (mEmptyFileNameObserver == null) return;
                        mEmptyFileNameObserver.onResult(getTargetName().isEmpty());
                    }
                });
    }

    /**
     * @param callback Callback to run when edit text is empty.
     */
    public void setEmptyInputObserver(Callback<Boolean> callback) {
        mEmptyFileNameObserver = callback;
    }

    /**
     * @param renameResult RenameResult to distinguish dialog type, used for updating the subtitle
     *         view.
     * @param suggestedName The content to update the display on EditText box.
     */
    public void updateToErrorView(String suggestedName, @RenameResult int renameResult) {
        if (renameResult == RenameResult.SUCCESS) return;

        setEditText(suggestedName);
        setEditTextStyle(true);
        setErrorMessageVisibility(true);
        switch (renameResult) {
            case RenameResult.FAILURE_NAME_CONFLICT:
                mErrorMessageView.setText(R.string.rename_failure_name_conflict);
                break;
            case RenameResult.FAILURE_NAME_TOO_LONG:
                mErrorMessageView.setText(R.string.rename_failure_name_too_long);
                break;
            case RenameResult.FAILURE_NAME_INVALID:
                mErrorMessageView.setText(R.string.rename_failure_name_invalid);
                break;
            case RenameResult.FAILURE_UNAVAILABLE:
                mErrorMessageView.setText(R.string.rename_failure_unavailable);
                break;
            default:
                break;
        }
    }

    /**
     * @param suggestedName String value display in the EditTextBox.
     * Initialize components in view: hide subtitle and reset value in the editTextBox.
     */
    public void initializeView(String suggestedName) {
        setEditText(suggestedName);
        setEditTextStyle(false);
        setErrorMessageVisibility(false);
    }

    /**
     * @return A String from user input for the target name.
     */
    public String getTargetName() {
        return mFileName.getText().toString();
    }

    private void highlightEditText(String name) {
        highlightEditText(0, name.length() - RenameUtils.getFileExtension(name).length());
    }

    /**
     * @param startIndex Start index of selection.
     * @param endIndex End index of selection.
     * Select renamable title portion, require focus and acquire the soft keyboard.
     */
    private void highlightEditText(int startIndex, int endIndex) {
        mFileName.setOnFocusChangeListener(
                new OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        if (!hasFocus) return;

                        if (endIndex <= 0
                                || startIndex > endIndex
                                || endIndex >= getTargetName().length() - 1) {
                            mFileName.selectAll();
                        } else {
                            mFileName.setSelection(startIndex, endIndex);
                        }
                    }
                });

        post(this::showSoftInput);
    }

    private void showSoftInput() {
        if (mFileName.requestFocus()) {
            InputMethodManager inputMethodManager =
                    (InputMethodManager)
                            mFileName.getContext().getSystemService(INPUT_METHOD_SERVICE);
            inputMethodManager.showSoftInput(mFileName, InputMethodManager.SHOW_FORCED);
        }
    }

    private void setErrorMessageVisibility(boolean hasError) {
        mErrorMessageView.setTextColor(getContext().getColor(R.color.default_text_color_error));
        mErrorMessageView.setVisibility(hasError ? View.VISIBLE : View.GONE);
    }

    private void setEditText(String suggestedName) {
        if (!TextUtils.isEmpty(suggestedName)) {
            mFileName.setText(suggestedName);
        }
        mFileName.clearFocus();
        highlightEditText(suggestedName);
    }

    private void setEditTextStyle(boolean hasError) {
        if (hasError) {
            // Change the edit text box underline tint color.
            mFileName
                    .getBackground()
                    .setColorFilter(
                            getContext().getColor(R.color.default_red), PorterDuff.Mode.SRC_IN);
        } else {
            mFileName.getBackground().clearColorFilter();
        }
    }
}
