// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.content.Context;
import android.content.res.Resources;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * Creates the AllPasswordsBottomSheet. AllPasswordsBottomSheet uses a bottom sheet to let the
 * user select a credential and fills it into the focused form.
 */
class AllPasswordsBottomSheetCoordinator {
    private final AllPasswordsBottomSheetMediator mMediator = new AllPasswordsBottomSheetMediator();

    /**
     * This delegate is called when the AllPasswordsBottomSheet is interacted with (e.g. dismissed
     * or a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user selects one of the credentials shown in the AllPasswordsBottomSheet.
         */
        void onCredentialSelected(Credential credential);

        /**
         * Called when the user dismisses the AllPasswordsBottomSheet or if the bottom sheet content
         * failed to be shown.
         */
        void onDismissed();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles select and dismiss events.
     * @param origin The origin for the current focused frame.
     */
    public void initialize(Context context, BottomSheetController sheetController,
            AllPasswordsBottomSheetCoordinator.Delegate delegate, String origin) {
        PropertyModel model = AllPasswordsBottomSheetProperties.createDefaultModel(
                mMediator::onDismissed, mMediator::onQueryTextChange);
        mMediator.initialize(
                new ModalDialogManager(new AppModalPresenter(context), ModalDialogType.APP),
                getWarningDialogModel(context, mMediator, origin), delegate, model);
        setUpModelChangeProcessor(model, new AllPasswordsBottomSheetView(context, sheetController));
    }

    /**
     * Displays the given credentials in a new bottom sheet.
     * @param credentials An array of {@link Credential}s that will be displayed.
     * @param isPasswordField True if the currently focused field is a password field and false for
     *         any other field type (e.g username, ...).
     */
    public void showCredentials(Credential[] credentials, boolean isPasswordField) {
        mMediator.setCredentials(credentials, isPasswordField);
        mMediator.warnAndShow();
    }

    @VisibleForTesting
    static void setUpModelChangeProcessor(PropertyModel model, AllPasswordsBottomSheetView view) {
        PropertyModelChangeProcessor.create(
                model, view, AllPasswordsBottomSheetViewBinder::bindAllPasswordsBottomSheet);
    }

    private PropertyModel getWarningDialogModel(
            Context context, AllPasswordsBottomSheetMediator mediator, String origin) {
        View contentView = createWarningDialogView(context, origin);
        Resources resources = context.getResources();

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mediator)
                .with(ModalDialogProperties.TITLE, resources,
                        R.string.all_passwords_bottom_sheet_warning_dialog_title)
                .with(ModalDialogProperties.CUSTOM_VIEW, contentView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                        R.string.all_passwords_bottom_sheet_accept)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                        R.string.all_passwords_bottom_sheet_cancel)
                .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                .build();
    }

    private View createWarningDialogView(Context context, String origin) {
        LinearLayout contentView = (LinearLayout) LayoutInflater.from(context).inflate(
                R.layout.all_passwords_bottom_sheet_warning_dialog, null);
        TextView warningTextView = contentView.findViewById(R.id.warning_text_view);

        Resources resources = context.getResources();
        String formattedOrigin = UrlFormatter.formatUrlForSecurityDisplay(
                new GURL(origin), SchemeDisplay.OMIT_CRYPTOGRAPHIC);
        String message = String.format(
                resources.getString(
                        R.string.all_passwords_bottom_sheet_warning_dialog_message_first),
                formattedOrigin);

        int startIndex = message.indexOf(formattedOrigin);
        int endIndex = startIndex + formattedOrigin.length();
        Spannable spannableMessage = new SpannableString(message);
        StyleSpan boldStyle = new StyleSpan(android.graphics.Typeface.BOLD);
        spannableMessage.setSpan(
                boldStyle, startIndex, endIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        warningTextView.setText(spannableMessage);
        return contentView;
    }
}