// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND;
import static android.view.accessibility.AccessibilityNodeInfo.EXPANDED_STATE_COLLAPSED;
import static android.view.accessibility.AccessibilityNodeInfo.EXPANDED_STATE_FULL;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.widget.TextViewWithLeading;

@NullMarked
public class ContextMenuHeaderTextView extends LinearLayout {
    private boolean mCanExpandOrCollapse;
    private boolean mIsExpanded;
    private @Nullable OnClickListener mClickListener;

    private TextViewWithLeading mTitle;
    private TextViewWithLeading mUrl;
    private TextViewWithLeading mSecondaryUrl;
    private TextViewWithLeading mTertiaryUrl;

    public ContextMenuHeaderTextView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.menu_header_title);
        mUrl = findViewById(R.id.menu_header_url);
        mSecondaryUrl = findViewById(R.id.menu_header_secondary_url);
        mTertiaryUrl = findViewById(R.id.menu_header_tertiary_url);
        addOnLayoutChangeListener(
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> update());
        update();
    }

    public void setIsExpanded(boolean isExpanded) {
        mIsExpanded = isExpanded;
    }

    /** Returns whether the context menu header can currently be expanded. */
    private boolean isExpandable() {
        return isTextViewEllipsized(mTitle)
                || isTextViewEllipsized(mUrl)
                || isTextViewEllipsized(mSecondaryUrl)
                || isTextViewEllipsized(mTertiaryUrl);
    }

    /** Update this in response to a layout change of a contained {@link TextViewWithLeading}. */
    private void update() {
        mCanExpandOrCollapse |= isExpandable();
        if (mCanExpandOrCollapse && !hasOnClickListeners()) {
            // If we determine that the click listener would have a visible effect, attach it.
            super.setOnClickListener(mClickListener);
        }
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        if (mCanExpandOrCollapse) {
            info.addAction(mIsExpanded ? ACTION_COLLAPSE : ACTION_EXPAND);
            // TODO(crbug.com/435252704): Clean this up if AccessibilityNodeInfoCompat gets updated.
            if (Build.VERSION.SDK_INT >= 36) {
                info.setExpandedState(mIsExpanded ? EXPANDED_STATE_FULL : EXPANDED_STATE_COLLAPSED);
            }
        }
    }

    @Override
    public boolean performAccessibilityAction(int action, @Nullable Bundle arguments) {
        if (mCanExpandOrCollapse
                && (action == ACTION_EXPAND.getId() || action == ACTION_COLLAPSE.getId())) {
            performClick();
            return true;
        }
        return super.performAccessibilityAction(action, arguments);
    }

    @Override
    public void setOnClickListener(@Nullable OnClickListener listener) {
        // To avoid actually setting a click listener onto the view if the click listener has no
        // discernible effect, we store the listener for later.
        mClickListener = listener;
        if (
        // If we already know that the header is expandable, register the click listener right away.
        (mCanExpandOrCollapse && listener != null)
                ||
                // If expand / collapse is not allowed, we can set the click listener to null.
                (!mCanExpandOrCollapse && listener == null)) {
            super.setOnClickListener(listener);
        }
    }

    private static boolean isTextViewEllipsized(TextViewWithLeading textView) {
        // If there's nothing in the text, it's not ellipsized.
        if (TextUtils.isEmpty(textView.getText()) || textView.getLayout() == null) return false;
        // Check if the text is ellipsized on its last line.
        return textView.getLayout().getEllipsisCount(textView.getLineCount() - 1) > 0;
    }
}
