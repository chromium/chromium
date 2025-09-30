// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import android.content.Context;
import android.content.res.Resources;
import android.text.Editable;
import android.text.InputFilter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView.BufferType;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.DialogMode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToView;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.ViewToMediator;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.ArrayList;
import java.util.List;

/** The View of the Custom Tile Edit Dialog. */
@NullMarked
class CustomTileEditView extends FrameLayout
        implements ModalDialogProperties.Controller, MediatorToView {
    private final PropertyModel mDialogModel;
    private final List<Runnable> mOnWindowFocusTasks = new ArrayList<>();

    private TextInputEditText mNameField;
    private TextInputEditText mUrlField;
    private ViewToMediator mMediatorDelegate;

    /**
     * Constructor for inflation from XML.
     *
     * @param context The Android context.
     * @param atts The XML attributes.
     */
    public CustomTileEditView(Context context, AttributeSet atts) {
        super(context, atts);
        mDialogModel = createDialogModel();
    }

    // FrameLayout override.
    @Override
    @Initializer
    public void onFinishInflate() {
        super.onFinishInflate();
        mNameField = findViewById(R.id.name_field);
        mNameField.setFilters(
                new InputFilter[] {
                    new InputFilter.LengthFilter(SuggestionsConfig.MAX_CUSTOM_TILES_NAME_LENGTH)
                });
        mUrlField = findViewById(R.id.url_field);
        mUrlField.setFilters(
                new InputFilter[] {
                    new InputFilter.LengthFilter(SuggestionsConfig.MAX_CUSTOM_TILES_URL_LENGTH)
                });
        mUrlField.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        mMediatorDelegate.onUrlTextChanged(s.toString());
                    }
                });
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (hasWindowFocus && getVisibility() == View.VISIBLE) {
            for (Runnable task : mOnWindowFocusTasks) {
                task.run();
            }
            mOnWindowFocusTasks.clear();
        }
    }

    /**
     * Sets the delegate for handling user interactions.
     *
     * @param mediatorDelegate The delegate to set interaction events to.
     */
    @Initializer
    void setMediatorDelegate(ViewToMediator mediatorDelegate) {
        assert mMediatorDelegate == null;
        mMediatorDelegate = mediatorDelegate;
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel modelDialogModel, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            @Nullable Editable name = mNameField.getText();
            @Nullable Editable urlText = mUrlField.getText();
            mMediatorDelegate.onSave(
                    (name == null) ? "" : name.toString(),
                    (urlText == null) ? "" : urlText.toString());
        } else {
            mMediatorDelegate.onCancel();
        }
    }

    @Override
    public void onDismiss(PropertyModel modelDialogModel, int dismissalCause) {}

    // MediatorToView implementation.
    @Override
    public void addOnWindowFocusGainedTask(Runnable task) {
        mOnWindowFocusTasks.add(task);
    }

    @Override
    public void setDialogMode(@DialogMode int mode) {
        mDialogModel.set(ModalDialogProperties.TITLE, getDialogTitleFromMode(mode));
    }

    @Override
    public void setName(String name) {
        mNameField.setText(name, BufferType.EDITABLE);
    }

    @Override
    public void setUrlErrorByCode(@UrlErrorCode int urlErrorCode) {
        String message = null;
        if (urlErrorCode == UrlErrorCode.INVALID_URL) {
            message = getContext().getString(R.string.ntp_custom_links_invalid_url);
        } else if (urlErrorCode == UrlErrorCode.DUPLICATE_URL) {
            message = getContext().getString(R.string.ntp_custom_links_already_exists);
        }
        mUrlField.setError(message);
    }

    @Override
    public void setUrlText(String urlText) {
        mUrlField.setText(urlText, BufferType.EDITABLE);
    }

    @Override
    public void toggleSaveButton(boolean enable) {
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !enable);
    }

    @Override
    public void focusOnName() {
        mNameField.requestFocus();
        @Nullable Editable name = mNameField.getText();
        mNameField.setSelection((name == null) ? 0 : name.length());
        KeyboardUtils.showKeyboard(mNameField);
    }

    @Override
    public void focusOnUrl(boolean selectAll) {
        mUrlField.requestFocus();
        if (selectAll) {
            mUrlField.selectAll();
        }
        KeyboardUtils.showKeyboard(mUrlField);
    }

    PropertyModel getDialogModel() {
        return mDialogModel;
    }

    private PropertyModel createDialogModel() {
        Resources res = getContext().getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        res.getString(R.string.edit_shortcut_button_save))
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, res.getString(R.string.cancel))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .with(ModalDialogProperties.CUSTOM_VIEW, this)
                .build();
    }

    private String getDialogTitleFromMode(@DialogMode int mode) {
        Resources resources = getContext().getResources();
        switch (mode) {
            case DialogMode.ADD_SHORTCUT:
                return resources.getString(R.string.edit_shortcut_title_add);
            case DialogMode.EDIT_SHORTCUT:
                return resources.getString(R.string.edit_shortcut_title_edit);
        }
        assert false;
        return "";
    }
}
