// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import static org.chromium.chrome.browser.autofill_assistant.AssistantAccessibilityUtils.setAccessibility;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.prefeditor.EditorFieldModel;
import org.chromium.chrome.browser.autofill.prefeditor.EditorTextField;
import org.chromium.chrome.browser.autofill_assistant.AssistantChevronStyle;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpander;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpanderAccordion;
import org.chromium.ui.widget.ChromeImageView;

/** Generic view factory. */
@JNINamespace("autofill_assistant")
public class AssistantViewFactory {
    /** Attaches {@code view} to {@code container}. */
    @CalledByNative
    public static void addViewToContainer(ViewGroup container, View view) {
        container.addView(view);
    }

    /** Set view attributes. All padding values are in dp. */
    @CalledByNative
    public static void setViewAttributes(View view, Context context, int paddingStart,
            int paddingTop, int paddingEnd, int paddingBottom,
            @Nullable AssistantDrawable background, @Nullable String contentDescription,
            boolean visible, boolean enabled) {
        view.setPaddingRelative(AssistantDimension.getPixelSizeDp(context, paddingStart),
                AssistantDimension.getPixelSizeDp(context, paddingTop),
                AssistantDimension.getPixelSizeDp(context, paddingEnd),
                AssistantDimension.getPixelSizeDp(context, paddingBottom));
        if (background != null) {
            background.getDrawable(context, result -> {
                if (result != null) {
                    view.setBackground(result);
                }
            });
        }
        setAccessibility(view, contentDescription);
        view.setVisibility(visible ? View.VISIBLE : View.GONE);
        view.setEnabled(enabled);
    }

    /**
     * Sets layout parameters for {@code view}. {@code width} and {@code height} must bei either
     * MATCH_PARENT (-1), WRAP_CONTENT (-2) or a value in dp.
     */
    @CalledByNative
    public static void setViewLayoutParams(View view, Context context, int width, int height,
            float weight, int marginStart, int marginTop, int marginEnd, int marginBottom,
            int layoutGravity, int minimumWidth, int minimumHeight) {
        if (width > 0) {
            width = AssistantDimension.getPixelSizeDp(context, width);
        }
        if (height > 0) {
            height = AssistantDimension.getPixelSizeDp(context, height);
        }

        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(width, height);
        layoutParams.weight = weight;
        layoutParams.setMarginStart(AssistantDimension.getPixelSizeDp(context, marginStart));
        layoutParams.setMarginEnd(AssistantDimension.getPixelSizeDp(context, marginEnd));
        layoutParams.topMargin = AssistantDimension.getPixelSizeDp(context, marginTop);
        layoutParams.bottomMargin = AssistantDimension.getPixelSizeDp(context, marginBottom);
        layoutParams.gravity = layoutGravity;
        view.setLayoutParams(layoutParams);
        view.setMinimumWidth(AssistantDimension.getPixelSizeDp(context, minimumWidth));
        view.setMinimumHeight(AssistantDimension.getPixelSizeDp(context, minimumHeight));
    }

    /** Creates a {@code android.widget.LinearLayout} widget. */
    @CalledByNative
    public static LinearLayout createLinearLayout(
            Context context, String identifier, int orientation) {
        LinearLayout linearLayout = new LinearLayout(context);
        linearLayout.setOrientation(orientation);
        linearLayout.setTag(identifier);
        return linearLayout;
    }

    /** Creates a {@code android.widget.TextView} widget. */
    @CalledByNative
    public static TextView createTextView(Context context, AssistantGenericUiDelegate delegate,
            String identifier, String text, @Nullable String textAppearance, int textGravity) {
        TextView textView = new TextView(context);
        AssistantViewInteractions.setViewText(textView, text, delegate);
        textView.setTag(identifier);
        if (textAppearance != null) {
            int styleId = context.getResources().getIdentifier(
                    textAppearance, "style", context.getPackageName());
            if (styleId != 0) {
                ApiCompatibilityUtils.setTextAppearance(textView, styleId);
            }
        }
        textView.setGravity(textGravity);
        return textView;
    }

    /** Creates a divider widget as used in the {@code AssistantCollectUserData} action. */
    @CalledByNative
    public static View createDividerView(Context context, String identifier) {
        View divider = LayoutUtils.createInflater(context).inflate(
                org.chromium.chrome.autofill_assistant.R.layout
                        .autofill_assistant_payment_request_section_divider,
                null, false);
        divider.setTag(identifier);
        return divider;
    }

    /** Creates a {@code ChromeImageView} widget. */
    @CalledByNative
    public static ChromeImageView createImageView(
            Context context, String identifier, AssistantDrawable image) {
        ChromeImageView imageView = new ChromeImageView(context);
        imageView.setTag(identifier);
        image.getDrawable(context, result -> {
            if (result != null) {
                imageView.setImageDrawable(result);
            }
        });
        return imageView;
    }

    /** Creates a {@code EditorTextField} view. */
    @CalledByNative
    public static View createTextInputView(Context context, AssistantGenericUiDelegate delegate,
            String viewIdentifier, int type, String hint, String modelIdentifier) {
        View view = new EditorTextField(context,
                EditorFieldModel.createTextInput(type, hint, /* suggestions = */ null,
                        /* formatter = */ null, /* validator = */ null,
                        /* valueIconGenerator = */ null, /* requiredErrorMessage = */ null,
                        /* invalidErrorMessage = */ null,
                        EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, ""),
                (v, actionId, event)
                        -> false,
                /* filter = */ null, new TextWatcher() {
                    @Override
                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                    }

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable value) {
                        delegate.onValueChanged(modelIdentifier,
                                AssistantValue.createForStrings(new String[] {value.toString()}));
                    }
                });
        view.setTag(viewIdentifier);
        return view;
    }

    /**
     * Creates an {@code AssistantVerticalExpander} widget.
     * @param chevronStyle Should match the enum defined in
     *         components/autofill_assistant/browser/view_layout.proto
     */
    @CalledByNative
    public static AssistantVerticalExpander createVerticalExpander(Context context,
            String identifier, @Nullable View titleView, @Nullable View collapsedView,
            @Nullable View expandedView, @AssistantChevronStyle int chevronStyle) {
        AssistantVerticalExpander expander = new AssistantVerticalExpander(context, null);
        if (titleView != null) {
            expander.setTitleView(titleView,
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        if (collapsedView != null) {
            expander.setCollapsedView(collapsedView,
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        if (expandedView != null) {
            expander.setExpandedView(expandedView,
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        expander.setChevronStyle(chevronStyle);
        expander.setFixed(collapsedView == null || expandedView == null);
        expander.setTag(identifier);
        return expander;
    }

    /** Creates an {@code AssistantVerticalExpanderAccordion} widget. */
    @CalledByNative
    public static AssistantVerticalExpanderAccordion createVerticalExpanderAccordion(
            Context context, String identifier, int orientation) {
        AssistantVerticalExpanderAccordion accordion =
                new AssistantVerticalExpanderAccordion(context, null);
        accordion.setOrientation(orientation);
        accordion.setTag(identifier);
        return accordion;
    }

    /** Creates a {@code CompoundButton} widget. */
    @CalledByNative
    public static View createToggleButton(Context context, AssistantGenericUiDelegate delegate,
            String identifier, @Nullable View leftContentView, @Nullable View rightContentView,
            boolean isCheckbox, String modelIdentifier) {
        AssistantToggleButton view = new AssistantToggleButton(context,
                result
                -> delegate.onValueChanged(
                        modelIdentifier, AssistantValue.createForBooleans(new boolean[] {result})),
                leftContentView, rightContentView, isCheckbox);
        return view;
    }
}
