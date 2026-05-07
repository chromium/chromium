// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ACCEPT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ALL_KEYS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.DECLINE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.SETTINGS_LINK_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.VIDEO_LINK_CALLBACK;

import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;

/** This class is used to show the PIX account linking prompt. */
@NullMarked
public class PixAccountLinkingPrompt implements FacilitatedPaymentsSequenceView {
    private static final String VARIATION_B = "VariationB";

    private LinearLayout mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        String promptVariant =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.ENABLE_PIX_ACCOUNT_LINKING_NATIVE, "prompt_variant");

        int layoutId = R.layout.pix_account_linking_prompt;
        if (VARIATION_B.equals(promptVariant)) {
            layoutId = R.layout.pix_account_linking_prompt_b;
        }

        mView =
                (LinearLayout)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(layoutId, viewContainer, false);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public PropertyModel getModel() {
        PropertyModel model = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, mView, PixAccountLinkingPrompt::bind);
        return model;
    }

    // The Pix account linking prompt isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ACCEPT_BUTTON_CALLBACK) {
            ButtonCompat acceptButton = view.findViewById(R.id.accept_button);
            acceptButton.setOnClickListener(model.get(ACCEPT_BUTTON_CALLBACK));
        } else if (propertyKey == DECLINE_BUTTON_CALLBACK) {
            ButtonCompat declineButton = view.findViewById(R.id.decline_button);
            declineButton.setOnClickListener(model.get(DECLINE_BUTTON_CALLBACK));
        } else if (propertyKey == SETTINGS_LINK_CALLBACK) {
            TextView settingsLink = view.findViewById(R.id.pix_code_detection_settings_link);
            if (settingsLink != null) {
                settingsLink.setText(
                        SpanApplier.applySpans(
                                settingsLink
                                        .getContext()
                                        .getString(
                                                R.string
                                                        .pix_account_linking_prompt_b_settings_link),
                                new SpanApplier.SpanInfo(
                                        "<link1>",
                                        "</link1>",
                                        new ChromeClickableSpan(
                                                settingsLink.getContext(),
                                                v -> model.get(SETTINGS_LINK_CALLBACK).onClick(v)) {
                                            @Override
                                            public void updateDrawState(
                                                    android.text.TextPaint textPaint) {
                                                super.updateDrawState(textPaint);
                                                textPaint.setUnderlineText(false);
                                            }
                                        })));
                settingsLink.setMovementMethod(LinkMovementMethod.getInstance());
            }
        } else if (propertyKey == VIDEO_LINK_CALLBACK) {
            TextView valueProp1 = view.findViewById(R.id.value_prop_message_1);
            if (valueProp1 != null) {
                valueProp1.setText(
                        SpanApplier.applySpans(
                                valueProp1
                                        .getContext()
                                        .getString(
                                                R.string
                                                        .pix_account_linking_prompt_b_value_prop_message_1),
                                new SpanApplier.SpanInfo(
                                        "<link1>",
                                        "</link1>",
                                        new ChromeClickableSpan(
                                                valueProp1.getContext(),
                                                v -> model.get(VIDEO_LINK_CALLBACK).onClick(v)) {
                                            @Override
                                            public void updateDrawState(
                                                    android.text.TextPaint textPaint) {
                                                super.updateDrawState(textPaint);
                                                textPaint.setUnderlineText(false);
                                            }
                                        })));
                valueProp1.setMovementMethod(LinkMovementMethod.getInstance());
            }
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
