// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;

/**
 * Base class for defining methods where different behavior is required by downstream targets.
 * The correct version of {@link LocaleManagerDelegateImpl} will be determined at compile time
 * via build rules.
 */
public class LocaleManagerDelegate {
    private static final String SPECIAL_LOCALE_ID = "US";

    private static final int SNACKBAR_DURATION_MS = 6000;

    private boolean mSearchEnginePromoShownThisSession;
    private boolean mSearchEnginePromoCheckedThisSession;

    // LocaleManager is a singleton and it should not have strong reference to UI objects.
    // SnackbarManager is owned by ChromeActivity and is not null as long as the activity is alive.
    private WeakReference<SnackbarManager> mSnackbarManager = new WeakReference<>(null);
    private LocaleTemplateUrlLoader mLocaleTemplateUrlLoader;
    @Nullable
    private SettingsLauncher mSettingsLauncher;

    private SnackbarController mSnackbarController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {}

        @Override
        public void onAction(Object actionData) {

        }
    };

    /**
     * Default constructor.
     */
    public LocaleManagerDelegate() {
    }

    /**
     * @see {@link LocaleManager#startObservingPhoneChanges()}
     */
    public void startObservingPhoneChanges() {
        maybeAutoSwitchSearchEngine();
    }

    /**
     * @see {@link LocaleManager#stopObservingPhoneChanges()}
     */
    public void stopObservingPhoneChanges() {}

    /**
     * @see {@link LocaleManager#recordStartupMetrics()}
     */
    public void recordStartupMetrics() {}

    /**
     * @see {@link LocaleManager#isSpecialLocaleEnabled()}
     */
    public boolean isSpecialLocaleEnabled() {
        return false;
    }

    /**
     * @see {@link LocaleManager#getSpecialLocaleId()}
     */
    public String getSpecialLocaleId() {
        return SPECIAL_LOCALE_ID;
    }

    /**
     * @see {@link LocaleManager#addSpecialSearchEngines()}
     */
    public void addSpecialSearchEngines() {
        if (!isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().loadTemplateUrls();
    }

    /**
     * Overrides the default search engine to a different search engine we designate. This is a
     * no-op if the user has manually changed DSP settings.
     */
    void overrideDefaultSearchEngine() {
        if (!isSearchEngineAutoSwitchEnabled() || !isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().overrideDefaultSearchProvider();
        showSnackbar(ContextUtils.getApplicationContext().getString(R.string.using_sogou));
    }

    /**
     * Reverts the temporary change made in {@link #overrideDefaultSearchEngine()}. This is a no-op
     * if the user has manually changed DSP settings.
     */
    private void revertDefaultSearchEngineOverride() {
        if (!isSearchEngineAutoSwitchEnabled() || isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().setGoogleAsDefaultSearch();
        showSnackbar(ContextUtils.getApplicationContext().getString(R.string.using_google));
    }

    /**
     * @see {@link LocaleManager#maybeAutoSwitchSearchEngine()}
     */
    protected void maybeAutoSwitchSearchEngine() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        boolean wasInSpecialLocale = preferences.readBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE, false);
        boolean isInSpecialLocale = isSpecialLocaleEnabled();
        if (wasInSpecialLocale && !isInSpecialLocale) {
            revertDefaultSearchEngineOverride();
        } else if (isInSpecialLocale && !wasInSpecialLocale) {
            addSpecialSearchEngines();
            overrideDefaultSearchEngine();
        } else if (isInSpecialLocale) {
            // As long as the user is in the special locale, special engines should be in the list.
            addSpecialSearchEngines();
        }
        preferences.writeBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE, isInSpecialLocale);
    }

    /**
     * Called when search engine to use is selected on SogouPromoDialog.
     * @param useSogou {@code true} if Sogou engine is chosen.
     */
    private void onSelectSearchEngine(boolean useSogou) {
        setSearchEngineAutoSwitch(useSogou);
        addSpecialSearchEngines();
        if (useSogou) overrideDefaultSearchEngine();
    }

    /**
     * @return Whether auto switch for search engine is enabled.
     */
    private boolean isSearchEngineAutoSwitchEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, false);
    }

    /**
     * @see {@link LocaleManager#setSearchEngineAutoSwitch()}
     */
    public void setSearchEngineAutoSwitch(boolean isEnabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, isEnabled);
    }

    /**
     * @see {@link LocaleManager#setSnackbarManager()}
     */
    public void setSnackbarManager(SnackbarManager manager) {
        mSnackbarManager = new WeakReference<SnackbarManager>(manager);
    }

    /**
     * @see {@link LocaleManager#setSettingsLauncher()}
     */
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    private void showSnackbar(CharSequence title) {
        SnackbarManager manager = mSnackbarManager.get();
        if (manager == null) return;

        Context context = ContextUtils.getApplicationContext();
        Snackbar snackbar = Snackbar.make(title, mSnackbarController, Snackbar.TYPE_NOTIFICATION,
                Snackbar.UMA_SPECIAL_LOCALE);
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        snackbar.setAction(context.getString(R.string.settings), null);
        manager.showSnackbar(snackbar);
    }

    /**
     * @see {@link LocaleManager#getYandexReferralId()}
     */
    public String getYandexReferralId() {
        return "";
    }

    /**
     * @see {@link LocaleManager#getMailRUReferralId()}
     */
    public String getMailRUReferralId() {
        return "";
    }

    private LocaleTemplateUrlLoader getLocaleTemplateUrlLoader() {
        if (mLocaleTemplateUrlLoader == null) {
            mLocaleTemplateUrlLoader = new LocaleTemplateUrlLoader(getSpecialLocaleId());
        }
        return mLocaleTemplateUrlLoader;
    }

    /**
     * @see {@link LocaleManager#recordLocaleBasedSearchWidgetMetrics()}
     */
    public void recordLocaleBasedSearchWidgetMetrics(boolean widgetPresent) {}

    /**
     * @see {@link LocaleManager#hasShownSearchEnginePromoThisSession()}
     */
    public boolean hasShownSearchEnginePromoThisSession() {
        return mSearchEnginePromoShownThisSession;
    }

    /**
     * @see {@link LocaleManager#recordLocaleBasedSearchMetrics()}
     */
    public void recordLocaleBasedSearchMetrics(
            boolean isFromSearchWidget, String url, @PageTransition int transition) {}

    /**
     * @see {@link LocaleManager#isSpecialUser()}
     */
    public boolean isSpecialUser() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_SPECIAL_USER);
    }

    /**
     * @see {@link LocaleManager#recordUserTypeMetrics()}
     */
    public void recordUserTypeMetrics() {}
}
