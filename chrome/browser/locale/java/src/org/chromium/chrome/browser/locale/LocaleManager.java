// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.base.PageTransition;

/**
 * Manager for some locale specific logics.
 * TODO(https://crbug.com/1198923) Turn this into a per-activity object.
 */
public class LocaleManager {
    private static LocaleManager sInstance = new LocaleManager();

    private LocaleManagerDelegate mDelegate;

    /**
     * @return An instance of the {@link LocaleManager}. This should only be called on UI thread.
     */
    @CalledByNative
    public static LocaleManager getInstance() {
        assert ThreadUtils.runningOnUiThread();
        return sInstance;
    }

    /**
     * Default constructor.
     */
    public LocaleManager() {
        mDelegate = new LocaleManagerDelegateImpl();
    }

    /**
     * Starts listening to state changes of the phone.
     */
    public void startObservingPhoneChanges() {
        mDelegate.startObservingPhoneChanges();
    }

    /**
     * Stops listening to state changes of the phone.
     */
    public void stopObservingPhoneChanges() {
        mDelegate.stopObservingPhoneChanges();
    }

    /**
     * Starts recording metrics in deferred startup.
     */
    public void recordStartupMetrics() {
        mDelegate.recordStartupMetrics();
    }

    /** Returns whether the Chrome instance is running in a special locale. */
    public boolean isSpecialLocaleEnabled() {
        return mDelegate.isSpecialLocaleEnabled();
    }

    /**
     * @return The country id of the special locale.
     */
    public String getSpecialLocaleId() {
        return mDelegate.getSpecialLocaleId();
    }

    /**
     * Adds local search engines for special locale.
     */
    public void addSpecialSearchEngines() {
        mDelegate.addSpecialSearchEngines();
    }

    /**
     * Sets the {@link SnackbarManager} used by this instance.
     * @param manager SnackbarManager instance.
     */
    public void setSnackbarManager(SnackbarManager manager) {
        mDelegate.setSnackbarManager(manager);
    }

    /**
     * Sets the settings launcher for search engines.
     * @param settingsLauncher Launcher to start search engine settings on the snackbar UI.
     */
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mDelegate.setSettingsLauncher(settingsLauncher);
    }

    /**
     * @return The referral ID to be passed when searching with Yandex as the DSE.
     */
    @CalledByNative
    protected String getYandexReferralId() {
        return mDelegate.getYandexReferralId();
    }

    /**
     * @return The referral ID to be passed when searching with Mail.RU as the DSE.
     */
    @CalledByNative
    protected String getMailRUReferralId() {
        return mDelegate.getMailRUReferralId();
    }

    /** Set a LocaleManager instance. This is called only by AppHooks. */
    public static void setInstance(LocaleManager instance) {
        sInstance = instance;
    }

    /**
     * Record any locale based metrics related with the search widget. Recorded on initialization
     * only.
     * @param widgetPresent Whether there is at least one search widget on home screen.
     */
    public void recordLocaleBasedSearchWidgetMetrics(boolean widgetPresent) {
        mDelegate.recordLocaleBasedSearchWidgetMetrics(widgetPresent);
    }

    /** Returns whether the search engine promo has been shown in this session. */
    public boolean hasShownSearchEnginePromoThisSession() {
        return mDelegate.hasShownSearchEnginePromoThisSession();
    }

    /** Returns whether we still have to check for whether search engine dialog is necessary. */
    public boolean needToCheckForSearchEnginePromo() {
        return false;
    }

    /**
     * Record any locale based metrics related with search. Recorded per search.
     * @param isFromSearchWidget Whether the search was performed from the search widget.
     * @param url Url for the search made.
     * @param transition The transition type for the navigation.
     */
    public void recordLocaleBasedSearchMetrics(
            boolean isFromSearchWidget, String url, @PageTransition int transition) {
        mDelegate.recordLocaleBasedSearchMetrics(isFromSearchWidget, url, transition);
    }

    /**
     * Returns whether the user requires special handling.
     */
    public boolean isSpecialUser() {
        return mDelegate.isSpecialUser();
    }

    /**
     * Record metrics related to user type.
     */
    @CalledByNative
    public void recordUserTypeMetrics() {
        mDelegate.recordUserTypeMetrics();
    }

}
