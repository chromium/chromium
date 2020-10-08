// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider.TintObserver;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The home button.
 */
public class HomeButton extends ChromeImageButton
        implements TintObserver, OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener,
                   HomepageManager.HomepageStateListener {
    @VisibleForTesting
    public static final int ID_REMOVE = 0;
    @VisibleForTesting
    public static final int ID_SETTINGS = 1;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** The {@link ActivityTabTabObserver} used to know when the active page changed. */
    private ActivityTabTabObserver mActivityTabTabObserver;

    /** The {@link ActivityTabProvider} used to know if the active tab is on the NTP. */
    private ActivityTabProvider mActivityTabProvider;

    // Test related members
    private static boolean sSaveContextMenuForTests;
    private ContextMenu mMenuForTests;

    private SettingsLauncher mSettingsLauncher;

    private ObservableSupplier<StartSurface> mStartSurfaceSupplier;
    private Callback<StartSurface> mStartSurfaceSupplierObserver;
    private StartSurface mStartSurface;
    private StartSurface.StateObserver mStartSurfaceStateObserver;

    public HomeButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        final int homeButtonIcon = R.drawable.btn_toolbar_home;
        setImageDrawable(ContextCompat.getDrawable(context, homeButtonIcon));
        HomepageManager.getInstance().addListener(this);
        updateContextMenuListener();

        mSettingsLauncher = new SettingsLauncherImpl();
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

        if (mStartSurfaceSupplier != null) {
            mStartSurfaceSupplier.removeObserver(mStartSurfaceSupplierObserver);
        }

        if (mStartSurface != null) {
            mStartSurface.removeStateChangeObserver(mStartSurfaceStateObserver);
        }
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    public void setStartSurfaceSupplier(ObservableSupplier<StartSurface> startSurfaceSupplier) {
        assert ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage();

        mStartSurfaceSupplier = startSurfaceSupplier;
        mStartSurfaceSupplierObserver = (startSurface) -> {
            // TODO(crbug.com/1084528): Replace with OneShotSupplier when it is available.
            assert startSurface != null;
            assert mStartSurface == null : "The StartSurface should be set at most once.";

            mStartSurface = startSurface;
            mStartSurfaceStateObserver = (newState, shouldShowToolbar) -> {
                if (newState == StartSurfaceState.SHOWN_HOMEPAGE) {
                    updateButtonEnabledState(false);
                } else {
                    updateButtonEnabledState(null);
                }
            };
            startSurface.addStateChangeObserver(mStartSurfaceStateObserver);
        };
        mStartSurfaceSupplier.addObserver(mStartSurfaceSupplierObserver);
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        // Disable long click before native initialized.
        if (!ChromeFeatureList.isInitialized()) return;

        if (isHomepageSettingsUIConversionEnabled()) {
            menu.add(Menu.NONE, ID_SETTINGS, Menu.NONE, R.string.options_homepage_edit_title)
                    .setOnMenuItemClickListener(this);
        } else {
            menu.add(Menu.NONE, ID_REMOVE, Menu.NONE, R.string.remove)
                    .setOnMenuItemClickListener(this);
        }

        if (sSaveContextMenuForTests) mMenuForTests = menu;
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        assert !isManagedByPolicy();
        if (isHomepageSettingsUIConversionEnabled()) {
            assert item.getItemId() == ID_SETTINGS;
            mSettingsLauncher.launchSettingsActivity(getContext(), HomepageSettings.class);
        } else {
            assert item.getItemId() == ID_REMOVE;
            HomepageManager.getInstance().setPrefHomepageEnabled(false);
        }

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
            public void onObservingDifferentTab(Tab tab, boolean hint) {
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

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    /**
     * Menu button is enabled when not in NTP or if in NTP and homepage is enabled and set to
     * somewhere other than the NTP.
     * @param tab The notifying {@link Tab} that might be selected soon, this is a hint that a tab
     *         change is likely.
     */
    public void updateButtonEnabledState(Tab tab) {
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
        updateButtonEnabledState(isEnabled);
    }

    private void updateButtonEnabledState(boolean isEnabled) {
        setEnabled(isEnabled);
        updateContextMenuListener();
    }

    /**
     * Check if the provided tab is NTP. The tab is a hint that
     * @param tab The notifying {@link Tab} that might be selected soon, this is a hint that a tab
     *         change is likely.
     */
    private boolean isTabNTP(Tab tab) {
        return tab != null && NewTabPage.isNTPUrl(tab.getUrlString());
    }

    /**
     * Return the active tab. If no active tab is shown, return null.
     */
    private Tab getActiveTab() {
        if (mActivityTabProvider == null) return null;

        return mActivityTabProvider.get();
    }

    private boolean isManagedByPolicy() {
        return HomepagePolicyManager.isHomepageManagedByPolicy();
    }

    private void updateContextMenuListener() {
        if (!isManagedByPolicy()) {
            setOnCreateContextMenuListener(this);
        } else {
            setOnCreateContextMenuListener(null);
            setLongClickable(false);
        }
    }

    private boolean isHomepageSettingsUIConversionEnabled() {
        assert ChromeFeatureList.isInitialized();
        return ChromeFeatureList.isEnabled(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION);
    }

    /**
     * @param saveContextMenuForTests Whether we want to store the context menu for testing
     */
    @VisibleForTesting
    public static void setSaveContextMenuForTests(boolean saveContextMenuForTests) {
        sSaveContextMenuForTests = saveContextMenuForTests;
    }

    /**
     * @return Latest context menu created.
     */
    @VisibleForTesting
    public ContextMenu getMenuForTests() {
        return mMenuForTests;
    }

    @VisibleForTesting
    public void setSettingsLauncherForTests(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }
}
