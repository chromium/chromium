// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.ui.widget.Toast;

/**
 * A button displaying the number of open tabs. Clicking the button toggles the tab switcher view.
 * TODO(twellington): Replace with TabSwitcherButtonCoordinator so code can be shared with bottom
 *                    toolbar.
 */
public class ToggleTabStackButton
        extends ListMenuButton implements TabCountProvider.TabCountObserver, View.OnClickListener,
                                          View.OnLongClickListener {
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;
    private TabSwitcherDrawable mTabSwitcherButtonDrawableLight;
    private TabCountProvider mTabCountProvider;
    private OnClickListener mTabSwitcherListener;
    private OnLongClickListener mTabSwitcherLongClickListener;
    private PulseDrawable mHighlightDrawable;
    private Drawable mNormalBackground;

    public ToggleTabStackButton(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherButtonDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), false);
        mTabSwitcherButtonDrawableLight =
                TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), true);
        setImageDrawable(mTabSwitcherButtonDrawable);
        setOnClickListener(this);
        setOnLongClickListener(this);
    }

    /**
     * Called to destroy the tab stack button.
     */
    void destroy() {
        if (mTabCountProvider != null) mTabCountProvider.removeObserver(this);
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(OnClickListener listener) {
        mTabSwitcherListener = listener;
    }

    /**
     * Sets the OnLongClickListern that will be notified when the TabSwitcher button is long
     *         pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is long
     *         pressed.
     */
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {
        mTabSwitcherLongClickListener = listener;
    }

    /**
     * Updates the contained drawable.
     * @param useLightDrawables Whether light drawables should be used.
     */
    void setUseLightDrawables(boolean useLightDrawables) {
        setImageDrawable(
                useLightDrawables ? mTabSwitcherButtonDrawableLight : mTabSwitcherButtonDrawable);
    }

    /**
     * @param provider The {@link TabCountProvider} used to observe the number of tabs in the
     *                 current model.
     */
    void setTabCountProvider(TabCountProvider provider) {
        mTabCountProvider = provider;
        mTabCountProvider.addObserverAndTrigger(this);
    }

    @Override
    public void onTabCountChanged(int numberOfTabs, boolean isIncognito) {
        setEnabled(numberOfTabs >= 1);
        setContentDescription(getResources().getQuantityString(
                R.plurals.accessibility_toolbar_btn_tabswitcher_toggle, numberOfTabs,
                numberOfTabs));
        mTabSwitcherButtonDrawableLight.updateForTabCount(numberOfTabs, isIncognito);
        mTabSwitcherButtonDrawable.updateForTabCount(numberOfTabs, isIncognito);
    }

    @Override
    public void onClick(View v) {
        if (mTabSwitcherListener != null && isClickable()) {
            mTabSwitcherListener.onClick(this);
        }
    }

    @Override
    public boolean onLongClick(View v) {
        if (mTabSwitcherLongClickListener != null && isLongClickable()) {
            return mTabSwitcherLongClickListener.onLongClick(v);
        } else {
            CharSequence description =
                    getResources().getString(org.chromium.chrome.R.string.open_tabs);
            return Toast.showAnchoredToast(getContext(), v, description);
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("ToggleTabStackButton.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("ToggleTabStackButton.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    // Approach copied from menu_button/MenuButton.java. Needed because ViewHighlighter centers on
    // the bounding box, while this button is slightly askew.
    public void setHighlightDrawable(boolean highlighting) {
        // TODO(https://crbug.com/1130752): Setting mNormalBackground is done here to keep this new
        // logic implicitly flag guarded. After merged, move this to #onFinishInflate().
        if (mNormalBackground == null) {
            mNormalBackground = getBackground();
        }

        if (highlighting) {
            if (mHighlightDrawable == null) {
                mHighlightDrawable = PulseDrawable.createCircle(getContext());
                mHighlightDrawable.setInset(ViewCompat.getPaddingStart(this), this.getPaddingTop(),
                        ViewCompat.getPaddingEnd(this), this.getPaddingBottom());
            }
            boolean useLightDrawables = getDrawable() == mTabSwitcherButtonDrawableLight;
            mHighlightDrawable.setUseLightPulseColor(
                    getContext().getResources(), useLightDrawables);
            setBackground(mHighlightDrawable);
            mHighlightDrawable.start();
        } else {
            setBackground(mNormalBackground);
        }
    }
}
