// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Space;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSection.Delegate;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A container for additional sections of the user data form. */
public class AssistantAdditionalSectionContainer {
    private final Context mContext;
    private final ViewGroup mParent;
    private final View mPlaceholderView;
    private final List<AssistantAdditionalSection> mSections = new ArrayList<>();
    private final List<View> mDividers = new ArrayList<>();
    private Delegate mDelegate;

    public AssistantAdditionalSectionContainer(Context context, ViewGroup parent) {
        mContext = context;
        mParent = parent;
        mPlaceholderView = new Space(context, null);
        parent.addView(mPlaceholderView);
    }

    public void setSections(List<AssistantAdditionalSectionFactory> sections) {
        for (AssistantAdditionalSection section : mSections) {
            mParent.removeView(section.getView());
        }
        for (View divider : mDividers) {
            mParent.removeView(divider);
        }

        mSections.clear();
        int index = mParent.indexOfChild(mPlaceholderView);
        LayoutInflater inflater = LayoutInflater.from(mContext);
        for (int i = sections.size() - 1; i >= 0; i--) {
            View divider = inflater.inflate(
                    R.layout.autofill_assistant_payment_request_section_divider, mParent, false);
            divider.setTag(AssistantCollectUserDataCoordinator.DIVIDER_TAG);
            mParent.addView(divider, index);
            AssistantAdditionalSection section =
                    sections.get(i).createSection(mContext, mParent, index, mDelegate);
            section.setDelegate(mDelegate);
            mSections.add(section);
        }
        Collections.reverse(mSections);
    }

    /**
     * Sets the paddings for all contained sections.
     *
     * @param topPadding padding for the top-most view in this container.
     * @param sectionPadding inter-section padding, i.e., padding between one section and the next
     *         divider, in either direction.
     * @param bottomPadding padding for the bottom-most view in this container.
     */
    public void setPaddings(int topPadding, int sectionPadding, int bottomPadding) {
        if (mSections.isEmpty()) {
            return;
        }

        if (mSections.size() == 1) {
            mSections.get(0).setPaddings(topPadding, bottomPadding);
            return;
        }

        mSections.get(0).setPaddings(topPadding, sectionPadding);
        mSections.get(mSections.size() - 1).setPaddings(sectionPadding, bottomPadding);
        for (int i = 1; i < mSections.size() - 1; i++) {
            mSections.get(i).setPaddings(sectionPadding, sectionPadding);
        }
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
        for (AssistantAdditionalSection section : mSections) {
            section.setDelegate(mDelegate);
        }
    }
}
