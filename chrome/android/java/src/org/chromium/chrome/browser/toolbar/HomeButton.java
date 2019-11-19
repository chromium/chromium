// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v4.content.ContextCompat;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The home button.
 */
public class HomeButton extends ChromeImageButton
        implements TintObserver, OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener,
                   HomepageManager.HomepageStateListener {
    private static final int ID_REMOVE = 0;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** The {@link ActivityTabTabObserver} used to know when the active page changed. */
    private ActivityTabTabObserver mActivityTabTabObserver;

    /** The {@link ActivityTabProvider} used to know if the active tab is on the NTP. */
    private ActivityTabProvider mActivityTabProvider;

    /** The home button text label. */
    private TextView mLabel;

    /** The wrapper View that contains the home button and the label. */
    private View mWrapper;

    public HomeButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        final int homeButtonIcon = R.drawable.btn_toolbar_home;
        setImageDrawable(ContextCompat.getDrawable(context, homeButtonIcon));
        if (!FeatureUtilities.isBottomToolbarEnabled()) {
            setOnCreateContextMenuListener(this);
        }

        HomepageManager.getInstance().addListener(this);
    }

    /**
     * @param wrapper The wrapping View of this button.
     */
    public void setWrapperView(ViewGroup wrapper) {
        mWrapper = wrapper;
        mLabel = mWrapper.findViewById(R.id.home_button_label);
        if (FeatureUtilities.isLabeledBottomToolbarEnabled()) mLabel.setVisibility(View.VISIBLE);
    }

    @Override
    public void setOnClickListener(OnClickListener listener) {
        if (mWrapper != null) {
            mWrapper.setOnClickListener(listener);
        } else {
            super.setOnClickListener(listener);
        }
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }

        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }

        HomepageManager.getInstance().removeListener(this);
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
        if (mLabel != null) mLabel.setTextColor(tint);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        menu.add(Menu.NONE, ID_REMOVE, Menu.NONE, R.string.remove).setOnMenuItemClickListener(this);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        assert item.getItemId() == ID_REMOVE;
        HomepageManager.getInstance().setPrefHomepageEnabled(false);
        return true;
    }

    @Override
    public void onHomepageStateUpdated() {
        updateButtonEnabledState(null);
    }

    public void setActivityTabProvider(ActivityTabProvider activityTabProvider) {
        mActivityTabProvider = activityTabProvider;
        mActivityTabTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab) {
                if (tab == null) return;
                updateButtonEnabledState(tab);
            }

            @Override
            public void onUpdateUrl(Tab tab, String url) {
                if (tab == null) return;
                updateButtonEnabledState(tab);
            }
        };
    }

    /**
     * Menu button is enabled when not in NTP or if in NTP and homepage is enabled and set to
     * somewhere other than the NTP.
     * @param tab The notifying {@link Tab} that might be selected soon, this is a hint that a tab
     *         change is likely.
     */
    private void updateButtonEnabledState(Tab tab) {
        // New tab page button takes precedence over homepage.
        final boolean isHomepageEnabled = HomepageManager.isHomepageEnabled();

        boolean isEnabled;
        if (getActiveTab() != null) {
            // Now tab shows a webpage, let's check if the webpage is not the NTP, or the webpage is
            // NTP but homepage is not NTP.
            isEnabled = !isTabNTP(getActiveTab())
                    || (isHomepageEnabled
                            && !NewTabPage.isNTPUrl(HomepageManager.getHomepageUri()));
        } else {
            // There is no active tab, which means tab is in transition, ex tab swither view to tab
            // view, or from one tab to another tab.
            isEnabled = !isTabNTP(tab);
        }
        setEnabled(isEnabled);
        if (mWrapper != null) mWrapper.setEnabled(isEnabled);
        if (mLabel != null) mLabel.setEnabled(isEnabled);
    }

    /**
     * Check if the provided tab is NTP. The tab is a hint that
     * @param tab The notifying {@link Tab} that might be selected soon, this is a hint that a tab
     *         change is likely.
     */
    private boolean isTabNTP(Tab tab) {
        return tab != null && NewTabPage.isNTPUrl(tab.getUrl());
    }

    /**
     * Return the active tab. If no active tab is shown, return null.
     */
    private Tab getActiveTab() {
        if (mActivityTabProvider == null) return null;

        return mActivityTabProvider.get();
    }
}
