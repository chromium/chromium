// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.tablet.emptybackground.incognitotoggle;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * A subclass of IncognitoToggleButton that adds some functionality to hide the button when no
 * incognito tabs are open. This subclass also implements the "toggle incognito mode on click"
 * behavior directly (the base class allows the code instantiating the button to set custom
 * behavior).
 */
public class IncognitoToggleButtonTablet extends IncognitoToggleButton {
    private TabModelObserver mTabModelObserver;

    /**
     * Creates an instance of {@link IncognitoToggleButtonTablet}.
     * @param context The {@link Context} to create this {@link View} under.
     * @param attrs An {@link AttributeSet} that contains information on how to build this
     *         {@link View}.
     */
    public IncognitoToggleButtonTablet(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        setVisibility(View.GONE);

        setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mTabModelSelector != null) {
                    mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
                }
            }
        });
    }

    @Override
    protected void setImage(boolean isIncognitoSelected) {
        setImageResource(isIncognitoSelected ? R.drawable.location_bar_incognito_badge
                                             : R.drawable.btn_tabstrip_switch_normal);
    }

    /**
     * Sets the {@link TabModelSelector} that will be queried for information about the state of
     * the system.
     * @param selector A {@link TabModelSelector} that represents the state of the system.
     */
    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        super.setTabModelSelector(selector);
        if (selector != null) {
            updateButtonVisibility();

            mTabModelObserver = new EmptyTabModelObserver() {
                @Override
                public void didAddTab(Tab tab, @TabLaunchType int type) {
                    updateButtonVisibility();
                }

                @Override
                public void willCloseTab(Tab tab, boolean animate) {
                    updateButtonVisibility();
                }

                @Override
                public void tabRemoved(Tab tab) {
                    updateButtonVisibility();
                }
            };
            for (TabModel model : mTabModelSelector.getModels()) {
                model.addObserver(mTabModelObserver);
            }
        }
    }

    private void updateButtonVisibility() {
        if (mTabModelSelector == null || mTabModelSelector.getCurrentModel() == null) {
            setVisibility(View.GONE);
            return;
        }

        post(new Runnable() {
            @Override
            public void run() {
                setVisibility(
                        mTabModelSelector.getModel(true).getCount() > 0 ? View.VISIBLE : View.GONE);
            }
        });
    }

    @Override
    protected void onAttachedToWindow() {
        if (mTabModelSelector != null) {
            mTabModelSelector.addObserver(mTabModelSelectorObserver);
            for (TabModel model : mTabModelSelector.getModels()) {
                model.addObserver(mTabModelObserver);
            }
        }
        super.onAttachedToWindow();
    }

    @Override
    protected void onDetachedFromWindow() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            for (TabModel model : mTabModelSelector.getModels()) {
                model.removeObserver(mTabModelObserver);
            }
        }
        super.onDetachedFromWindow();
    }
}
