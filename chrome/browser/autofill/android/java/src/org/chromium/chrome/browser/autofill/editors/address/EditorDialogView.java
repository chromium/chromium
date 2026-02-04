// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.address;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.ItemType.NON_EDITABLE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.isDropdownField;

import android.app.Activity;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsViewBinder;
import org.chromium.chrome.browser.autofill.editors.common.EditorViewBase;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldView;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldViewBinder;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldView;
import org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldViewBinder;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    // TODO(crbug.com/40265078): substitute this with SimpleRecyclerViewMCP.
    private final List<PropertyModelChangeProcessor<PropertyModel, TextFieldView, PropertyKey>>
            mTextFieldMCPs;
    private final List<PropertyModelChangeProcessor<PropertyModel, DropdownFieldView, PropertyKey>>
            mDropdownFieldMCPs;
    private final List<EditText> mEditableTextFields;
    private final List<Spinner> mDropdownFields;

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

        mTextFieldMCPs = new ArrayList<>();
        mDropdownFieldMCPs = new ArrayList<>();
        mEditableTextFields = new ArrayList<>();
        mDropdownFields = new ArrayList<>();
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

    /**
     * Create the visual representation of the PropertyModel defined by {@link EditorProperties}.
     *
     * <p>This would be more optimal as a RelativeLayout, but because it's dynamically generated,
     * it's much more human-parsable with inefficient LinearLayouts for half-width controls sharing
     * rows.
     *
     * @param editorFields the list of fields this editor should display.
     */
    @Override
    protected void prepareEditor(ListModel<EditorItem> editorFields) {
        // Ensure the layout is empty.
        clearFieldViews();
        mTextFieldMCPs.forEach(PropertyModelChangeProcessor::destroy);
        mDropdownFieldMCPs.forEach(PropertyModelChangeProcessor::destroy);
        mTextFieldMCPs.clear();
        mDropdownFieldMCPs.clear();
        mEditableTextFields.clear();
        mDropdownFields.clear();

        // Add Views for each of the {@link EditorFields}.
        for (int i = 0; i < editorFields.size(); i++) {
            EditorItem editorItem = editorFields.get(i);
            EditorItem nextEditorItem = null;

            boolean isLastField = i == editorFields.size() - 1;
            boolean useFullLine = editorItem.isFullLine;
            if (!isLastField && !useFullLine) {
                // If the next field isn't full, stretch it out.
                nextEditorItem = editorFields.get(i + 1);
                if (nextEditorItem.isFullLine) useFullLine = true;
            }

            // Always keep dropdowns and text fields on different lines because of height
            // differences.
            if (!isLastField
                    && !useFullLine
                    && isDropdownField(editorItem)
                            != isDropdownField(assumeNonNull(nextEditorItem))) {
                useFullLine = true;
            }

            if (useFullLine || isLastField) {
                addFieldViewToEditor(getContentView(), editorItem);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(getActivity());
                getContentView().addView(rowLayout);

                View firstView = addFieldViewToEditor(rowLayout, editorItem);
                View lastView = addFieldViewToEditor(rowLayout, assumeNonNull(nextEditorItem));

                LinearLayout.LayoutParams firstParams =
                        (LinearLayout.LayoutParams) firstView.getLayoutParams();
                LinearLayout.LayoutParams lastParams =
                        (LinearLayout.LayoutParams) lastView.getLayoutParams();

                firstParams.width = 0;
                firstParams.weight = 1;
                MarginLayoutParamsCompat.setMarginEnd(
                        firstParams,
                        getStyledContext()
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.editor_dialog_section_large_spacing));
                lastParams.width = 0;
                lastParams.weight = 1;

                i = i + 1;
            }
        }
    }

    private View addFieldViewToEditor(ViewGroup parent, final EditorItem editorItem) {
        View childView = null;

        switch (editorItem.type) {
            case DROPDOWN:
                {
                    DropdownFieldView dropdownView =
                            new DropdownFieldView(getStyledContext(), parent, editorItem.model);
                    mDropdownFieldMCPs.add(
                            PropertyModelChangeProcessor.create(
                                    editorItem.model,
                                    dropdownView,
                                    DropdownFieldViewBinder::bindDropdownFieldView));
                    addFieldView(dropdownView);
                    mDropdownFields.add(dropdownView.getDropdown());
                    childView = dropdownView.getLayout();
                    break;
                }
            case TEXT_INPUT:
                {
                    TextFieldView inputLayout =
                            new TextFieldView(getStyledContext(), editorItem.model);
                    mTextFieldMCPs.add(
                            PropertyModelChangeProcessor.create(
                                    editorItem.model,
                                    inputLayout,
                                    TextFieldViewBinder::bindTextFieldView));
                    addFieldView(inputLayout);
                    mEditableTextFields.add(inputLayout.getEditText());
                    childView = inputLayout;
                    break;
                }
            case NON_EDITABLE_TEXT:
                {
                    View textLayout =
                            LayoutInflater.from(getStyledContext())
                                    .inflate(
                                            R.layout.autofill_editor_dialog_non_editable_textview,
                                            null);
                    PropertyModelChangeProcessor.create(
                            editorItem.model,
                            textLayout,
                            EditorComponentsViewBinder::bindNonEditableTextView);
                    childView = textLayout;
                    break;
                }
            case NOTICE:
                {
                    View noticeLayout =
                            LayoutInflater.from(getStyledContext())
                                    .inflate(R.layout.autofill_editor_dialog_notice, null);
                    TextView textView = noticeLayout.findViewById(R.id.notice);
                    PropertyModelChangeProcessor.create(
                            editorItem.model,
                            textView,
                            EditorComponentsViewBinder::bindNoticeTextView);
                    childView = noticeLayout;
                    break;
                }
        }
        assumeNonNull(childView);
        parent.addView(childView);
        return childView;
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

    /**
     * @return All editable text fields in the editor. Used only for tests.
     */
    public List<EditText> getEditableTextFieldsForTest() {
        return mEditableTextFields;
    }

    /**
     * @return All dropdown fields in the editor. Used only for tests.
     */
    public List<Spinner> getDropdownFieldsForTest() {
        return mDropdownFields;
    }

    @Override
    protected void onEntryAnimationStart() {
        for (int i = 0; i < mEditableTextFields.size(); i++) {
            mEditableTextFields.get(i).setEnabled(false);
        }
    }

    @Override
    protected void onEntryAnimationEnd() {
        for (int i = 0; i < mEditableTextFields.size(); i++) {
            mEditableTextFields.get(i).setEnabled(true);
        }
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
