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

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;

/** A section which displays a simple static text message. */
public class AssistantStaticTextSection implements AssistantAdditionalSection {
    private final LinearLayout mRootLayout;
    private final int mTitleToContentPadding;

    /** Factory for instantiating instances of {code AssistantStaticTextSection}. */
    public static class Factory implements AssistantAdditionalSectionFactory {
        private final String mTitle;
        private final String mMessage;
        public Factory(String title, String message) {
            mTitle = title;
            mMessage = message;
        }

        @Override
        public AssistantStaticTextSection createSection(
                Context context, ViewGroup parent, int index, Delegate delegate) {
            return new AssistantStaticTextSection(context, parent, index, mTitle, mMessage);
        }
    }

    AssistantStaticTextSection(
            Context context, ViewGroup parent, int index, String title, String message) {
        mTitleToContentPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_title_padding);

        LayoutInflater inflater = LayoutInflater.from(context);
        mRootLayout = (LinearLayout) inflater.inflate(
                R.layout.autofill_assistant_static_text_section, null);

        TextView titleView = mRootLayout.findViewById(R.id.section_title);
        AssistantTextUtils.applyVisualAppearanceTags(titleView, title, null);

        TextView messageView = mRootLayout.findViewById(R.id.text);
        AssistantTextUtils.applyVisualAppearanceTags(messageView, message, null);

        parent.addView(mRootLayout, index,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    @Override
    public View getView() {
        return mRootLayout;
    }

    @Override
    public void setPaddings(int topPadding, int bottomPadding) {
        View titleView = mRootLayout.findViewById(R.id.section_title);
        titleView.setPadding(titleView.getPaddingLeft(), topPadding, titleView.getPaddingRight(),
                mTitleToContentPadding);
        TextView messageView = mRootLayout.findViewById(R.id.text);
        messageView.setPadding(messageView.getPaddingLeft(), messageView.getPaddingTop(),
                messageView.getPaddingRight(), bottomPadding);
    }

    @Override
    public void setDelegate(Delegate delegate) {
        // Nothing to do.
    }
}
