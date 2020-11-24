// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils.TruncateAt;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatImageView;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.core.widget.TextViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * View for Group Headers.
 */
public class HeaderView extends SimpleHorizontalLayoutView {
    private final TextView mHeaderText;
    private final ImageView mHeaderIcon;
    private boolean mIsCollapsed;
    private Runnable mOnSelectListener;

    /**
     * Constructs a new header view.
     *
     * @param context Current context.
     */
    public HeaderView(Context context) {
        super(context);

        TypedValue themeRes = new TypedValue();
        getContext().getTheme().resolveAttribute(R.attr.selectableItemBackground, themeRes, true);
        setBackgroundResource(themeRes.resourceId);
        setClickable(true);
        setFocusable(true);

        mHeaderText = new TextView(context);
        mHeaderText.setLayoutParams(LayoutParams.forDynamicView());
        mHeaderText.setMaxLines(1);
        mHeaderText.setEllipsize(TruncateAt.END);
        mHeaderText.setAllCaps(true);
        TextViewCompat.setTextAppearance(
                mHeaderText, ChromeColors.getTextMediumThickSecondaryStyle(false));
        mHeaderText.setMinHeight(context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height));
        mHeaderText.setGravity(Gravity.CENTER_VERTICAL);
        mHeaderText.setTextAlignment(TextView.TEXT_ALIGNMENT_VIEW_START);
        mHeaderText.setPaddingRelative(context.getResources().getDimensionPixelSize(
                                               R.dimen.omnibox_suggestion_header_margin_start),
                0, 0, 0);
        addView(mHeaderText);

        mHeaderIcon = new AppCompatImageView(context);
        mHeaderIcon.setScaleType(ImageView.ScaleType.CENTER);
        mHeaderIcon.setImageResource(R.drawable.ic_expand_more_black_24dp);
        mHeaderIcon.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_action_icon_width),
                LayoutParams.MATCH_PARENT));
        addView(mHeaderIcon);

        ViewCompat.setAccessibilityDelegate(this, new AccessibilityDelegateCompat() {
            @Override
            public void onInitializeAccessibilityNodeInfo(
                    View host, AccessibilityNodeInfoCompat info) {
                super.onInitializeAccessibilityNodeInfo(host, info);
                AccessibilityActionCompat action = mIsCollapsed
                        ? AccessibilityActionCompat.ACTION_EXPAND
                        : AccessibilityActionCompat.ACTION_COLLAPSE;

                info.addAction(new AccessibilityActionCompat(
                        AccessibilityEvent.TYPE_VIEW_CLICKED, action.getLabel()));
                info.addAction(action);
            }

            @Override
            public boolean performAccessibilityAction(View host, int action, Bundle arguments) {
                if (action == AccessibilityNodeInfoCompat.ACTION_EXPAND
                        || action == AccessibilityNodeInfoCompat.ACTION_COLLAPSE) {
                    return performClick();
                }
                return super.performAccessibilityAction(host, action, arguments);
            }
        });
    }

    /** Return ImageView used to present group header chevron. */
    public ImageView getIconView() {
        return mHeaderIcon;
    }

    /** Return TextView used to present group header text. */
    public TextView getTextView() {
        return mHeaderText;
    }

    /**
     * Specifies whether view should be announced as expanded or collapsed.
     *
     * @param isCollapsed true, if view should be announced as collapsed.
     */
    void setCollapsedStateForAccessibility(boolean isCollapsed) {
        mIsCollapsed = isCollapsed;
    }

    /**
     * Specify the listener receiving calls when the view is selected.
     */
    void setOnSelectListener(Runnable listener) {
        mOnSelectListener = listener;
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        if (selected && mOnSelectListener != null) {
            mOnSelectListener.run();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            return performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }
}
