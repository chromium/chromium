// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.RadioGroup;
import android.widget.RadioGroup.OnCheckedChangeListener;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * The custom view in the download later dialog.
 */
public class DownloadLaterDialogView extends ScrollView implements OnCheckedChangeListener {
    private Controller mController;

    private RadioButtonWithDescription mDownloadNow;
    private RadioButtonWithDescription mOnWifi;
    private RadioButtonWithDescription mDownloadLater;
    private RadioGroup mRadioGroup;
    private CheckBox mCheckBox;
    private TextView mEditText;

    /**
     * The view binder to propagate events from model to view.
     */
    public static class Binder {
        public static void bind(
                PropertyModel model, DownloadLaterDialogView view, PropertyKey propertyKey) {
            if (propertyKey == DownloadLaterDialogProperties.CONTROLLER) {
                view.setController(model.get(DownloadLaterDialogProperties.CONTROLLER));
            } else if (propertyKey == DownloadLaterDialogProperties.INITIAL_CHOICE) {
                view.setChoice(model.get(DownloadLaterDialogProperties.INITIAL_CHOICE));
            } else if (propertyKey == DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION) {
                view.setCheckbox(
                        model.get(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION));
            } else if (propertyKey == DownloadLaterDialogProperties.DONT_SHOW_AGAIN_DISABLED) {
                view.setCheckboxEnabled(
                        !model.get(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_DISABLED));
            } else if (propertyKey == DownloadLaterDialogProperties.LOCATION_TEXT) {
                view.setShowEditLocation(model.get(DownloadLaterDialogProperties.LOCATION_TEXT));
            }
        }
    }

    /**
     * Receives events in download later dialog.
     */
    public interface Controller {
        /**
         * Called when the edit location text is clicked.
         */
        void onEditLocationClicked();

        /**
         * Called when the choice radio buttons changed.
         * @param choice The choice that the user selected.
         */
        void onCheckedChanged(@DownloadLaterDialogChoice int choice);
    }

    public DownloadLaterDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initialize the internal objects.
     */
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDownloadNow = (RadioButtonWithDescription) findViewById(R.id.download_now);
        mOnWifi = (RadioButtonWithDescription) findViewById(R.id.on_wifi);
        mDownloadLater = (RadioButtonWithDescription) findViewById(R.id.choose_date_time);
        mRadioGroup = (RadioGroup) findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);
        mCheckBox = (CheckBox) findViewById(R.id.show_again_checkbox);
        mEditText = (TextView) findViewById(R.id.edit_location);
    }

    void setController(Controller controller) {
        mController = controller;
    }

    void setChoice(@DownloadLaterDialogChoice int choice) {
        switch (choice) {
            case DownloadLaterDialogChoice.DOWNLOAD_NOW:
                mDownloadNow.setChecked(true);
                break;
            case DownloadLaterDialogChoice.ON_WIFI:
                mOnWifi.setChecked(true);
                break;
            case DownloadLaterDialogChoice.DOWNLOAD_LATER:
                mDownloadLater.setChecked(true);
                break;
        }
    }

    void setCheckbox(@DownloadLaterPromptStatus int promptStatus) {
        mCheckBox.setVisibility(VISIBLE);
        mCheckBox.setChecked(promptStatus == DownloadLaterPromptStatus.DONT_SHOW);
    }

    void setCheckboxEnabled(boolean enabled) {
        mCheckBox.setEnabled(enabled);
    }

    Integer getPromptStatus() {
        if (mCheckBox.getVisibility() == View.GONE || !mCheckBox.isEnabled()) return null;

        return mCheckBox.isChecked() ? DownloadLaterPromptStatus.DONT_SHOW
                                     : DownloadLaterPromptStatus.SHOW_PREFERENCE;
    }

    void setShowEditLocation(@Nullable String locationText) {
        if (locationText == null) {
            mEditText.setVisibility(GONE);
            return;
        }

        final SpannableString editText;
        final SpannableStringBuilder directorySpanBuilder = new SpannableStringBuilder();
        directorySpanBuilder.append(locationText);
        directorySpanBuilder.setSpan(new StyleSpan(Typeface.BOLD), 0, locationText.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        mEditText.setMovementMethod(LinkMovementMethod.getInstance());
        NoUnderlineClickableSpan editSpan = new NoUnderlineClickableSpan(
                getResources(), (view) -> { onEditLocationClicked(); });
        editText = SpanApplier.applySpans(
                getResources().getString(R.string.download_later_edit_location, locationText),
                new SpanInfo("<b>", "</b>", directorySpanBuilder),
                new SpanInfo("<LINK2>", "</LINK2>", editSpan));
        mEditText.setText(editText);
        mEditText.setVisibility(VISIBLE);
    }

    private void onEditLocationClicked() {
        assert mController != null : "Please bind the controller first.";
        mController.onEditLocationClicked();
    }

    // RadioGroup.OnCheckedChangeListener overrides.
    @Override
    public void onCheckedChanged(RadioGroup radioGroup, int index) {
        @DownloadLaterDialogChoice
        int choice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        if (mDownloadNow.isChecked()) {
            choice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        } else if (mOnWifi.isChecked()) {
            choice = DownloadLaterDialogChoice.ON_WIFI;
        } else if (mDownloadLater.isChecked()) {
            choice = DownloadLaterDialogChoice.DOWNLOAD_LATER;
        }
        mController.onCheckedChanged(choice);
    }
}
