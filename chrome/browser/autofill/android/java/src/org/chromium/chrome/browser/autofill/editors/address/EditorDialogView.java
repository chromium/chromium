// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.address;

import android.app.Activity;
import android.os.Handler;
import android.widget.EditText;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.editors.common.EditorViewBase;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * The editor dialog. Can be used for editing contact information, shipping address, billing
 * address.
 *
 * <p>TODO(crbug.com/41363594): Move payment specific functionality to separate class.
 */
@NullMarked
public class EditorDialogView extends EditorViewBase {
    private final Handler mHandler;

    private @Nullable String mProfileRecordTypeSuffix;

    private boolean mValidateOnShow;

    @VisibleForTesting
    public static final String PROFILE_DELETED_HISTOGRAM = "Autofill.ProfileDeleted.Any.Total";

    @VisibleForTesting
    public static final String PROFILE_DELETED_SETTINGS_HISTOGRAM =
            "Autofill.ProfileDeleted.Settings.Total";

    /**
     * Builds the editor dialog.
     *
     * @param activity The activity on top of which the UI should be displayed.
     */
    public EditorDialogView(Activity activity) {
        super(activity);
        mHandler = new Handler();
    }

    /**
     * Sets the suffix to be appended to the profile deletion histogram.
     *
     * @param suffix The suffix to append, e.g., the profile's record type.
     */
    public void setProfileRecordTypeSuffix(@Nullable String suffix) {
        mProfileRecordTypeSuffix = suffix;
    }

    public void setValidateOnShow(boolean validateOnShow) {
        mValidateOnShow = validateOnShow;
    }

    @Override
    protected void initFocus() {
        mHandler.post(
                () -> {
                    List<FieldView> invalidViews = new ArrayList<>();
                    if (mValidateOnShow) {
                        for (FieldView view : getFieldViews()) {
                            if (!view.validate()) {
                                invalidViews.add(view);
                            }
                        }
                    }

                    // If TalkBack is enabled, we want to keep the focus at the top
                    // because the user would not learn about the elements that are
                    // above the focused field.
                    if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                        if (!invalidViews.isEmpty()) {
                            // Immediately focus the first invalid field to make it faster to edit.
                            invalidViews.get(0).scrollToAndFocus();
                        } else {
                            // Trigger default focus as it is not triggered automatically on Android
                            // P+.
                            getContainerView().requestFocus();
                        }
                    }
                    // Note that keyboard will not be shown for dropdown field since it's not
                    // necessary.
                    if (getCurrentFocus() != null) {
                        KeyboardVisibilityDelegate.getInstance().showKeyboard(getCurrentFocus());
                        // Put the cursor to the end of the text.
                        if (getCurrentFocus() instanceof EditText) {
                            EditText focusedEditText = (EditText) getCurrentFocus();
                            focusedEditText.setSelection(focusedEditText.getText().length());
                        }
                    }
                    if (sObserverForTest != null) sObserverForTest.onEditorReadyToEdit();
                });
    }

    @Override
    protected void recordDeletionHistogram(boolean deleted) {
        RecordHistogram.recordBooleanHistogram(PROFILE_DELETED_HISTOGRAM, deleted);
        RecordHistogram.recordBooleanHistogram(PROFILE_DELETED_SETTINGS_HISTOGRAM, deleted);

        if (mProfileRecordTypeSuffix != null && !mProfileRecordTypeSuffix.isEmpty()) {
            RecordHistogram.recordBooleanHistogram(
                    PROFILE_DELETED_HISTOGRAM + "." + mProfileRecordTypeSuffix, deleted);
            RecordHistogram.recordBooleanHistogram(
                    PROFILE_DELETED_SETTINGS_HISTOGRAM + "." + mProfileRecordTypeSuffix, deleted);
        }
    }
}
