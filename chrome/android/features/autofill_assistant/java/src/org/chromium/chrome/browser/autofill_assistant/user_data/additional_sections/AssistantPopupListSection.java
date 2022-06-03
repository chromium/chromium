// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantChevronStyle;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpander;
import org.chromium.content.browser.input.PopupItemType;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupItem;

import java.util.ArrayList;
import java.util.List;

/** A section that when tapped prompts a popup with a list of items to chose from. */
public class AssistantPopupListSection implements AssistantAdditionalSection {
    private final AssistantVerticalExpander mSectionExpander;
    private final View mSummaryView;
    private final int mTitleToContentPadding;
    private final TextView mErrorView;
    private Delegate mDelegate;
    private int[] mCurrentSelectionIndexes;
    private boolean mSelectionMandatory;
    private String[] mItems;

    /** Factory for instantiating instances of {@code AssistantPopupListSection}. */
    public static class Factory implements AssistantAdditionalSectionFactory {
        private final String mTitle;
        private final String mIdentifier;
        private final String[] mItems;
        private final int[] mInitialSelection;
        private final boolean mAllowMultiselect;
        private final boolean mSelectionMandatory;
        private final String mNoSelectionErrorMessage;

        public Factory(String title, String identifier, String[] items, int[] initialSelection,
                boolean allowMultiselect, boolean selectionMandatory,
                String noSelectionErrorMessage) {
            this.mTitle = title;
            this.mIdentifier = identifier;
            this.mItems = items;
            this.mInitialSelection = initialSelection;
            this.mAllowMultiselect = allowMultiselect;
            this.mSelectionMandatory = selectionMandatory;
            this.mNoSelectionErrorMessage = noSelectionErrorMessage;
        }

        @Override
        public AssistantPopupListSection createSection(
                Context context, ViewGroup parent, int index, Delegate delegate) {
            return new AssistantPopupListSection(context, parent, index, mTitle, mIdentifier,
                    mItems, mInitialSelection, mAllowMultiselect, mSelectionMandatory,
                    mNoSelectionErrorMessage);
        }
    }

    public AssistantPopupListSection(Context context, ViewGroup parent, int index, String title,
            String identifier, String[] items, int[] initialSelection, boolean allowMultiselect,
            boolean selectionMandatory, @Nullable String noSelectionErrorMessage) {
        mCurrentSelectionIndexes = initialSelection;
        mSelectionMandatory = selectionMandatory;
        mItems = items;

        mTitleToContentPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_title_padding);

        LayoutInflater inflater = LayoutUtils.createInflater(context);
        mSectionExpander = new AssistantVerticalExpander(context, null);
        View sectionTitle =
                inflater.inflate(R.layout.autofill_assistant_payment_request_section_title, null);
        sectionTitle.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
        TextView titleView = sectionTitle.findViewById(R.id.section_title);
        AssistantTextUtils.applyVisualAppearanceTags(titleView, title, null);
        mSummaryView = inflater.inflate(R.layout.autofill_assistant_popup_list_section, null);
        mErrorView = mSummaryView.findViewById(R.id.error_message);
        if (noSelectionErrorMessage != null) {
            setErrorMessage(noSelectionErrorMessage);
        }

        List<SelectPopupItem> popupItems = new ArrayList<>();
        for (String item : items) {
            popupItems.add(new SelectPopupItem(item, PopupItemType.ENABLED));
        }
        updateSummary();

        View.OnClickListener onClickListener = unusedView -> {
            SelectPopupDialog popupDialog = new SelectPopupDialog(context, (indices) -> {
                if (indices == null) {
                    return;
                }
                mCurrentSelectionIndexes = indices;
                mDelegate.onValueChanged(identifier, new AssistantValue(indices));
                updateSummary();
            }, popupItems, allowMultiselect, mCurrentSelectionIndexes);
            popupDialog.show();
        };

        mSectionExpander.getTitleAndChevronContainer().setOnClickListener(onClickListener);

        mSectionExpander.setTitleView(sectionTitle,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setCollapsedView(mSummaryView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Adjust margins such that title and collapsed views are indented.
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        setHorizontalMargins(sectionTitle, horizontalMargin, horizontalMargin);
        setHorizontalMargins(mSectionExpander.getChevronButton(), 0, horizontalMargin);
        setHorizontalMargins(mSummaryView, horizontalMargin, 0);

        mSectionExpander.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
        mSectionExpander.setFixed(true);
        mSectionExpander.setChevronStyle(AssistantChevronStyle.ALWAYS);

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
        View titleView = mSectionExpander.getTitleView();
        titleView.setPadding(titleView.getPaddingLeft(), topPadding, titleView.getPaddingRight(),
                mTitleToContentPadding);
        mSectionExpander.getCollapsedView().setPadding(
                mSectionExpander.getCollapsedView().getPaddingLeft(),
                mSectionExpander.getCollapsedView().getPaddingTop(),
                mSectionExpander.getCollapsedView().getPaddingRight(), bottomPadding);
    }

    @Override
    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }

    private void setErrorMessage(String errorMessage) {
        AssistantTextUtils.applyVisualAppearanceTags(mErrorView, errorMessage, null);
    }

    private void setSummary(@Nullable String selection) {
        TextView selectedItemView = mSummaryView.findViewById(R.id.current_selection);
        if (selection == null) {
            selectedItemView.setVisibility(View.GONE);
            if (mSelectionMandatory) {
                mErrorView.setVisibility(View.VISIBLE);
            }
        } else {
            mErrorView.setVisibility(View.GONE);
            selectedItemView.setVisibility(View.VISIBLE);
            selectedItemView.setText(selection);
        }
    }

    private void updateSummary() {
        if (mCurrentSelectionIndexes.length == 0) {
            setSummary(null);
        } else {
            setSummary(mItems[mCurrentSelectionIndexes[0]]);
        }
    }
}
