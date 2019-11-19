// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/**
 * This widget is a linear layout that automatically manages the expansion state of
 * AssistantVerticalExpanders.
 *
 * Specifically, this widget implements accordion-like behavior for child expanders: when one is
 * expanded, all others are automatically collapsed. Child expanders need not be immediate children
 * of this widget, they can be anywhere in the child view hierarchy.
 *
 * For non-expander children, this view container behaves just like a regular LinearLayout.
 */
public class AssistantVerticalExpanderAccordion extends LinearLayout {
    private final List<AssistantVerticalExpander> mExpanders;
    /** The currently expanded view. */
    private AssistantVerticalExpander mExpandedView;
    private Callback<AssistantVerticalExpander> mOnExpandedViewChangedListener;

    public AssistantVerticalExpanderAccordion(Context context, AttributeSet attrs) {
        super(context, attrs);
        mExpanders = new ArrayList<>();
        setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {
                recursiveAddExpanders(child);
            }

            @Override
            public void onChildViewRemoved(View parent, View child) {
                recursiveRemoveExpanders(child);
            }
        });
    }

    public void setOnExpandedViewChangedListener(Callback<AssistantVerticalExpander> listener) {
        mOnExpandedViewChangedListener = listener;
    }

    /**
     * Expands the specified view and collapses all others.
     */
    private void setExpandedView(AssistantVerticalExpander expander) {
        mExpandedView = expander;
        for (int i = 0; i < mExpanders.size(); i++) {
            mExpanders.get(i).setExpanded(mExpanders.get(i) == mExpandedView);
        }
        if (mOnExpandedViewChangedListener != null) {
            mOnExpandedViewChangedListener.onResult(mExpandedView);
        }
    }

    /**
     * Recursively searches |view| and its children and adds all expanders to the list of managed
     * expanders.
     */
    private void recursiveAddExpanders(View view) {
        if (view instanceof AssistantVerticalExpander) {
            AssistantVerticalExpander expander = (AssistantVerticalExpander) view;
            mExpanders.add(expander);
            expander.setOnExpansionStateChangedListener(expanded -> {
                if (!expanded || mExpandedView == expander) {
                    if (!expanded && mExpandedView == expander) {
                        mExpandedView = null;
                        if (mOnExpandedViewChangedListener != null) {
                            mOnExpandedViewChangedListener.onResult(null);
                        }
                    }
                    return;
                }
                setExpandedView(expander);
            });
            if (expander.isExpanded()) {
                setExpandedView(expander);
            }
            return;
        }

        if (!(view instanceof ViewGroup)) {
            return;
        }

        ViewGroup viewGroup = (ViewGroup) view;
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            recursiveAddExpanders(viewGroup.getChildAt(i));
        }
    }

    /**
     * Recursively searches |view| and its children and removes all expanders from the list of
     * managed expanders.
     */
    private void recursiveRemoveExpanders(View view) {
        if (view instanceof AssistantVerticalExpander) {
            AssistantVerticalExpander expander = (AssistantVerticalExpander) view;
            expander.setOnExpansionStateChangedListener(null);
            mExpanders.remove(expander);
        }

        if (!(view instanceof ViewGroup)) {
            return;
        }

        ViewGroup viewGroup = (ViewGroup) view;
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            recursiveRemoveExpanders(viewGroup.getChildAt(i));
        }
    }
}
