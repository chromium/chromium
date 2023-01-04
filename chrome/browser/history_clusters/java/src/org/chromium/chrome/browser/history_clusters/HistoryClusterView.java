// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class HistoryClusterView extends SelectableItemView<HistoryCluster> {
    @IntDef({ClusterViewAccessibilityState.CLICKABLE, ClusterViewAccessibilityState.COLLAPSIBLE,
            ClusterViewAccessibilityState.EXPANDABLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClusterViewAccessibilityState {
        int CLICKABLE = 0;
        int COLLAPSIBLE = 1;
        int EXPANDABLE = 2;
    }

    private DividerView mDividerView;
    @ClusterViewAccessibilityState
    int mAccessibilityState;
    /**
     * Constructor for inflating from XML.
     */
    public HistoryClusterView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDividerView = new DividerView(getContext(), null, 0, R.style.HorizontalDivider);
        mDividerView.addToParent(this, generateDefaultLayoutParams());
        mEndButtonView.setVisibility(GONE);
        mEndButtonView.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        mEndButtonView.setOnTouchListener(
                (v, event) -> HistoryClusterView.this.onTouchEvent(event));
        setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
                super.onInitializeAccessibilityNodeInfo(host, info);
                info.removeAction(AccessibilityAction.ACTION_CLICK);
                info.removeAction(AccessibilityAction.ACTION_EXPAND);
                info.removeAction(AccessibilityAction.ACTION_COLLAPSE);
                switch (mAccessibilityState) {
                    case ClusterViewAccessibilityState.CLICKABLE:
                        info.addAction(AccessibilityAction.ACTION_CLICK);
                        break;
                    case ClusterViewAccessibilityState.COLLAPSIBLE:
                        info.addAction(AccessibilityAction.ACTION_COLLAPSE);
                        break;
                    case ClusterViewAccessibilityState.EXPANDABLE:
                        info.addAction(AccessibilityAction.ACTION_EXPAND);
                        break;
                }
            }
        });
    }

    @Override
    protected void onClick() {}

    @Override
    protected @Nullable ColorStateList getDefaultStartIconTint() {
        return ColorStateList.valueOf(
                SemanticColorUtils.getDefaultIconColorSecondary(getContext()));
    }

    void setTitle(CharSequence text) {
        mTitleView.setText(text);
    }

    void setLabel(CharSequence text) {
        mDescriptionView.setText(text);
    }

    void setIconDrawable(Drawable drawable) {
        super.setStartIconDrawable(drawable);
    }

    public void setStartIconBackgroundRes(@DrawableRes int resId) {
        mStartIconView.setBackgroundResource(resId);
        ImageViewCompat.setImageTintList(mStartIconView, getDefaultStartIconTint());
    }

    void setEndButtonDrawable(Drawable drawable) {
        mEndButtonView.setVisibility(VISIBLE);
        mEndButtonView.setImageDrawable(drawable);
    }

    void setDividerVisibility(boolean visible) {
        mDividerView.setVisibility(visible ? VISIBLE : GONE);
    }

    void setHasThickDivider(boolean hasThickDivider) {
        mDividerView.setIsThickDivider(hasThickDivider);
        LayoutParams layoutParams = (LayoutParams) mContentView.getLayoutParams();
        if (hasThickDivider) {
            layoutParams.bottomMargin =
                    getResources().getDimensionPixelSize(R.dimen.divider_margin);
        } else {
            layoutParams.bottomMargin = 0;
        }

        ViewUtils.requestLayout(this, "HistoryClusterView.setHasThickDivider");
    }

    void setIconDrawableVisibility(int visibility) {
        mStartIconView.setVisibility(visibility);
    }

    public void setAccessibilityState(@ClusterViewAccessibilityState int accessibilityState) {
        mAccessibilityState = accessibilityState;
    }
}
