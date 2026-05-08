// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.app.Activity;
import android.content.Context;
import android.graphics.Paint;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.List;

/** Prompt that asks users to confirm saving an entity imported from a form submission. */
@NullMarked
public class AutofillAiSaveUpdateEntityPrompt implements EntityEditorCoordinator.Delegate {
    private final AutofillAiSaveUpdateEntityPromptController mController;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final PropertyModel mDialogModel;
    private final View mDialogView;
    private EntityEditorCoordinator mEntityEditor;
    private boolean mEditorClosingPending;

    /** Save prompt to confirm saving an entity imported from a form submission. */
    public AutofillAiSaveUpdateEntityPrompt(
            AutofillAiSaveUpdateEntityPromptController controller,
            ModalDialogManager modalDialogManager,
            Activity activity,
            Profile profile,
            EntityInstance entityInstance) {
        mController = controller;
        mModalDialogManager = modalDialogManager;
        mContext = activity;

        LayoutInflater inflater = LayoutInflater.from(activity);
        mDialogView = inflater.inflate(R.layout.autofill_ai_save_entity_prompt, null);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        modalDialogManager, this::onDismiss))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView);
        mDialogModel = builder.build();
        mEntityEditor = new EntityEditorCoordinator(activity, this, profile, entityInstance);
    }

    @Override
    public void onDone(
            EntityInstance entityInstance, int descriptionStringId, int acceptButtonStringId) {
        // TODO: crbug.com/509798874 - Send the updated entity instance to C++.
        mEditorClosingPending = true;
    }

    /** Shows the dialog for saving an address. */
    @CalledByNative
    @VisibleForTesting
    void show() {
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Creates the prompt for saving an address.
     *
     * @param windowAndroid the window to supply Android dependencies.
     * @param controller the controller to handle the interaction.
     * @return instance of the AutofillAiSaveUpdateEntityPrompt or null if the call failed.
     */
    @CalledByNative
    private static @Nullable AutofillAiSaveUpdateEntityPrompt create(
            WindowAndroid windowAndroid,
            AutofillAiSaveUpdateEntityPromptController controller,
            Profile browserProfile,
            @JniType("autofill::EntityInstanceAndroid") EntityInstance entityInstance) {
        @Nullable Activity activity = windowAndroid.getActivity().get();
        @Nullable ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (activity == null || modalDialogManager == null) return null;

        return new AutofillAiSaveUpdateEntityPrompt(
                controller, modalDialogManager, activity, browserProfile, entityInstance);
    }

    /**
     * Displays the dialog-specific properties.
     *
     * @param title the title of the dialog.
     * @param positiveButtonText the text on the positive button.
     * @param negativeButtonText the text on the negative button.
     */
    @CalledByNative
    @VisibleForTesting
    void setDialogDetails(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String positiveButtonText,
            @JniType("std::u16string") String negativeButtonText,
            boolean isWalletableEntity) {
        mDialogModel.set(ModalDialogProperties.TITLE, title);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText);
        mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText);
        if (isWalletableEntity) {
            mDialogModel.set(
                    ModalDialogProperties.TITLE_END_ICON,
                    AppCompatResources.getDrawable(mContext, R.drawable.google_wallet_24dp));
        }
    }

    @CalledByNative
    @VisibleForTesting
    void setEntityUpdateDetails(
            @JniType("std::vector<autofill::EntityAttributeUpdateDetails>")
                    List<EntityAttributeUpdateDetails> updateDetailsList,
            boolean isUpdatePrompt) {
        LinearLayout attributeList = mDialogView.findViewById(R.id.autofill_ai_attribute_infos);
        attributeList.removeAllViews();

        if (updateDetailsList.isEmpty()) {
            return;
        }
        int startIndex = 0;
        LayoutInflater inflater = LayoutInflater.from(mContext);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_AI_EDIT_ENTITIES_FROM_SAVE_UPDATE_PROMPT)) {
            startIndex = 1;
            View attributeInfoWithEditButton =
                    inflater.inflate(
                            R.layout.autofill_ai_attribute_info_with_edit_button,
                            attributeList,
                            /* attachToRoot= */ false);
            attributeList.addView(attributeInfoWithEditButton);
            setAttributeDetails(
                    attributeInfoWithEditButton, updateDetailsList.get(0), isUpdatePrompt);

            attributeInfoWithEditButton
                    .findViewById(R.id.edit_button)
                    .setOnClickListener(
                            v -> {
                                mEditorClosingPending = false;
                                mEntityEditor.showEditorDialog();
                            });
        }

        for (int i = startIndex; i < updateDetailsList.size(); i++) {
            View attributeInfo =
                    inflater.inflate(
                            R.layout.autofill_ai_attribute_info,
                            attributeList,
                            /* attachToRoot= */ false);
            attributeList.addView(attributeInfo);
            setAttributeDetails(attributeInfo, updateDetailsList.get(i), isUpdatePrompt);
        }
    }

    private void setAttributeDetails(
            View attributeInfo,
            EntityAttributeUpdateDetails updateDetails,
            boolean isUpdatePrompt) {
        TextView attributeName = attributeInfo.findViewById(R.id.attribute_name);
        TextView attributeValue = attributeInfo.findViewById(R.id.attribute_value);

        attributeName.setText(updateDetails.getAttributeName());
        attributeValue.setText(updateDetails.getAttributeValue());

        if (isUpdatePrompt) {
            setBadgeAndAxLabelsInUpdatePrompt(attributeInfo, updateDetails);
        }
    }

    private void setBadgeAndAxLabelsInUpdatePrompt(
            View attributeInfo, EntityAttributeUpdateDetails updateDetails) {
        switch (updateDetails.getUpdateType()) {
            case EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_ADDED:
                configureAddedAttribute(attributeInfo, updateDetails);
                break;
            case EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_UPDATED:
                configureUpdatedAttribute(attributeInfo, updateDetails);
                break;
            case EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_UNCHANGED:
                // No custom accessibility label needed.
                break;
            default:
                assert false : "Unhandled attribute update type: " + updateDetails.getUpdateType();
        }
    }

    private void configureAddedAttribute(
            View attributeInfo, EntityAttributeUpdateDetails updateDetails) {
        TextView attributeName = attributeInfo.findViewById(R.id.attribute_name);
        attributeName.setContentDescription(
                mContext.getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_new_attribute_accessibility_name)
                        .replace("$1", updateDetails.getAttributeName()));

        TextView attributeValue = attributeInfo.findViewById(R.id.attribute_value);
        attributeValue.setText(
                SpanApplier.applySpans(
                        mContext.getString(
                                        R.string
                                                .autofill_ai_save_or_update_entity_new_attribute_with_badge)
                                .replace("$1", updateDetails.getAttributeValue()),
                        new SpanInfo(
                                "<new>",
                                "</new>",
                                new SuperscriptSpan(),
                                new RelativeSizeSpan(0.6f),
                                new ForegroundColorSpan(
                                        SemanticColorUtils.getDefaultTextColorAccent1(mContext)))));
    }

    private void configureUpdatedAttribute(
            View attributeInfo, EntityAttributeUpdateDetails updateDetails) {
        TextView attributeName = attributeInfo.findViewById(R.id.attribute_name);
        attributeName.setContentDescription(
                mContext.getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_updated_attribute_accessibility_name)
                        .replace("$1", updateDetails.getAttributeName())
                        .replace("$2", updateDetails.getOldAttributeValue()));

        // Show the old attribute value with the strikethrough text and use corresponding
        // accessibility label for the attribute name text view.
        TextView oldAttributeValue = attributeInfo.findViewById(R.id.old_attribute_value);
        oldAttributeValue.setPaintFlags(
                oldAttributeValue.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        oldAttributeValue.setVisibility(View.VISIBLE);
        oldAttributeValue.setText(updateDetails.getOldAttributeValue());
    }

    /**
     * Sets the source notice message in the prompt.
     *
     * @param sourceNotice the full source notice string.
     * @param insertManageInfoLink whether to insert a link to manage info. This value will be false
     *     for local entities and true for entities stored in wallet. In the case it is true,
     *     `sourceNotice` is expected to have a `R.string.autofill_manage_your_info_link` substring,
     *     which will be turned into a clickable link.
     */
    @CalledByNative
    @VisibleForTesting
    void setSourceNotice(
            @JniType("std::u16string") String sourceNotice, boolean insertManageInfoLink) {
        TextView sourceNoticeView = mDialogView.findViewById(R.id.autofill_ai_entity_source_notice);
        if (TextUtils.isEmpty(sourceNotice)) {
            // The source notice can be empty if the C++ controller fails to retrieve the email
            // address of the user.
            sourceNoticeView.setVisibility(View.GONE);
            return;
        }

        if (!insertManageInfoLink) {
            // Local entity source notice doesn't need a link.
            sourceNoticeView.setText(sourceNotice);
            return;
        }

        CharSequence sourceNoticeWithLink =
                SpanApplier.applySpans(
                        sourceNotice,
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        mContext,
                                        view -> {
                                            mController.onWalletLinkClicked();
                                        })));
        sourceNoticeView.setText(sourceNoticeWithLink, TextView.BufferType.SPANNABLE);
        sourceNoticeView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    @VisibleForTesting
    void dismiss() {
        // The user can close the editor by clicking the "Cancel" or back button. This starts an
        // exit animation which dismissed the editor. The prompt stays displayed during this period.
        // The native side can try to dismiss the prompt during the time frame. Do not dismiss the
        // editor again in this case.
        if (!mEditorClosingPending && mEntityEditor.isShowing()) {
            mEntityEditor.dismiss();
        }
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                mController.onUserAccepted();
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                mController.onUserDeclined();
                break;
            case DialogDismissalCause.ACTION_ON_CONTENT:
            default:
                // No explicit user decision.
                break;
        }
        mController.onPromptDismissed();
    }

    void setEntityEditorForTesting(EntityEditorCoordinator entityEditor) {
        EntityEditorCoordinator oldValue = mEntityEditor;
        mEntityEditor = entityEditor;
        ResettersForTesting.register(() -> mEntityEditor = oldValue);
    }

    View getDialogViewForTesting() {
        return mDialogView;
    }
}
