// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections;

import static org.chromium.chrome.browser.autofill.prefeditor.EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.prefeditor.EditorFieldModel;
import org.chromium.chrome.browser.autofill.prefeditor.EditorTextField;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpander;

import java.util.ArrayList;
import java.util.List;

/** A section which displays one or multiple text inputs for the user to type in. */
public class AssistantTextInputSection implements AssistantAdditionalSection {
    private final AssistantVerticalExpander mSectionExpander;
    private final ViewGroup mInputContainer;
    private final Context mContext;
    private Delegate mDelegate;
    private int mTopPadding;
    private int mBottomPadding;
    private final List<TextView> mSummaryViews = new ArrayList<>();
    private final int mTitleToContentPadding;

    /** Factory for a single text input field. */
    public static class TextInputFactory {
        private final @AssistantTextInputType int mType;
        private final String mHint;
        private final String mValue;
        private final String mKey;

        public TextInputFactory(
                @AssistantTextInputType int type, String hint, String value, String key) {
            mType = type;
            mHint = hint;
            mValue = value;
            mKey = key;
        }

        EditorTextField createView(
                Context context, Callback<Pair<String, String>> changedCallback) {
            TextWatcher textWatcher = new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {}

                @Override
                public void afterTextChanged(Editable s) {
                    changedCallback.onResult(new Pair<>(mKey, s.toString()));
                }
            };
            int typeHint = 0; /** == INPUT_TYPE_HINT_NONE */
            switch (mType) {
                case AssistantTextInputType.INPUT_TEXT:
                    break;
                case AssistantTextInputType.INPUT_ALPHANUMERIC:
                    typeHint = INPUT_TYPE_HINT_ALPHA_NUMERIC;
                    break;
            }
            return new EditorTextField(context,
                    EditorFieldModel.createTextInput(typeHint, mHint, /* suggestions = */ null,
                            /* formatter = */ null, /* validator = */ null,
                            /* valueIconGenerator = */ null, /* requiredErrorMessage = */ null,
                            /* invalidErrorMessage = */ null,
                            EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, mValue),
                    (v, actionId, event)
                            -> false,
                    /* filter = */ null, textWatcher);
        }
    }

    /** Factory for instantiating instances of {code AssistantTextInputSection}. */
    public static class Factory implements AssistantAdditionalSectionFactory {
        private final String mTitle;
        private final List<TextInputFactory> mInputs;
        public Factory(String title, List<TextInputFactory> inputs) {
            mTitle = title;
            mInputs = inputs;
        }

        @Override
        public AssistantTextInputSection createSection(
                Context context, ViewGroup parent, int index, Delegate delegate) {
            return new AssistantTextInputSection(context, parent, index, mTitle, mInputs, delegate);
        }
    }

    AssistantTextInputSection(Context context, ViewGroup parent, int index, String title,
            List<TextInputFactory> inputs, @Nullable Delegate delegate) {
        mContext = context;
        mDelegate = delegate;
        mTitleToContentPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_title_padding);

        LayoutInflater inflater = LayoutUtils.createInflater(context);
        mSectionExpander = new AssistantVerticalExpander(context, null);
        View sectionTitle =
                inflater.inflate(R.layout.autofill_assistant_payment_request_section_title, null);
        sectionTitle.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
        TextView titleView = sectionTitle.findViewById(R.id.section_title);
        AssistantTextUtils.applyVisualAppearanceTags(titleView, title, null);

        mInputContainer = createInputContainer();
        LinearLayout summaryViews = new LinearLayout(context);
        summaryViews.setOrientation(LinearLayout.VERTICAL);
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        for (TextInputFactory input : inputs) {
            TextView summaryView = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    summaryView, R.style.TextAppearance_TextMedium_Secondary);
            EditorTextField inputView = input.createView(context, result -> {
                if (mDelegate == null) {
                    return;
                }
                summaryView.setText(result.second);
                summaryView.setVisibility(
                        TextUtils.isEmpty(result.second) ? View.GONE : View.VISIBLE);

                mDelegate.onValueChanged(
                        result.first, new AssistantValue(new String[] {result.second}));
            });
            inputView.getEditText().setOnFocusChangeListener((unusedView, hasFocus) -> {
                if (!hasFocus && mDelegate != null) {
                    mDelegate.onTextFocusLost();
                }
            });
            mInputContainer.addView(inputView);
            mSummaryViews.add(summaryView);
            summaryViews.addView(summaryView);
            setHorizontalMargins(summaryView, horizontalMargin, horizontalMargin);
            setHorizontalMargins(inputView, horizontalMargin, horizontalMargin);
        }
        mSectionExpander.setExpandedView(mInputContainer,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setCollapsedView(summaryViews,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setTitleView(sectionTitle,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Adjust margins such that title is indented, but expanded view is full-width.
        setHorizontalMargins(sectionTitle, horizontalMargin, horizontalMargin);
        setHorizontalMargins(mSectionExpander.getChevronButton(), 0, horizontalMargin);

        parent.addView(mSectionExpander, index,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    @Override
    public View getView() {
        return mSectionExpander;
    }

    @Override
    public void setPaddings(int topPadding, int bottomPadding) {
        mTopPadding = topPadding;
        mBottomPadding = bottomPadding;
        updatePaddings();
    }

    @Override
    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    private boolean isEmpty() {
        for (int i = 0; i < mSummaryViews.size(); i++) {
            if (mSummaryViews.get(i).length() > 0) {
                return false;
            }
        }
        return true;
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }

    private void updatePaddings() {
        View titleView = mSectionExpander.getTitleView();
        View chevronButton = mSectionExpander.getChevronButton();
        if (isEmpty()) {
            // Section is empty, i.e., the title is the bottom-most widget.
            setTopAndBottomPadding(titleView, mTopPadding, mBottomPadding);
            setTopAndBottomPadding(chevronButton, mTopPadding, mBottomPadding);
            setTopAndBottomPadding(mSectionExpander.getCollapsedView(),
                    mSectionExpander.getCollapsedView().getPaddingTop(), 0);
        } else if (mSectionExpander.isExpanded()) {
            // Section is expanded, i.e., the expanded widget is the bottom-most widget.
            setTopAndBottomPadding(titleView, mTopPadding, mTitleToContentPadding);
            setTopAndBottomPadding(chevronButton, mTopPadding, mTitleToContentPadding);
            // No need to set additional bottom padding, expanded sections have enough already.
        } else {
            // Section is non-empty and collapsed -> collapsed widget is the bottom-most widget.
            setTopAndBottomPadding(titleView, mTopPadding, mTitleToContentPadding);
            setTopAndBottomPadding(chevronButton, mTopPadding, mBottomPadding);
            setTopAndBottomPadding(mSectionExpander.getCollapsedView(),
                    mSectionExpander.getCollapsedView().getPaddingTop(), mBottomPadding);
        }
    }

    private void setTopAndBottomPadding(View view, int topPadding, int bottomPadding) {
        view.setPadding(view.getPaddingLeft(), topPadding, view.getPaddingRight(), bottomPadding);
    }

    private ViewGroup createInputContainer() {
        LinearLayout inputContainer = new LinearLayout(mContext, null);
        inputContainer.setOrientation(LinearLayout.VERTICAL);
        inputContainer.setBackgroundColor(ApiCompatibilityUtils.getColor(
                mContext.getResources(), R.color.payments_section_edit_background));
        return inputContainer;
    }
}
