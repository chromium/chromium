// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.os.Bundle;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.site_settings.CookieControlsServiceBridge;
import org.chromium.chrome.browser.site_settings.CookieControlsServiceBridge.CookieControlsServiceObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.content_settings.CookieControlsEnforcement;

/**
 * A manager for cookie controls related behaviour on the incognito description view.
 * This class communicates with a native CookieControlsService and provides
 * updates to its listening observers. In addition, it is expected that this
 * class will be registered as an OnCheckedChangeListener for a corresponding
 * cookie controls view.
 */
public class IncognitoCookieControlsManager
        implements CookieControlsServiceObserver, OnCheckedChangeListener, View.OnClickListener {
    /**
     * Interface for a class that wants to receive updates from this manager.
     */
    public interface Observer {
        /**
         * Notifies that this manager has received an update.
         * @param checked A boolean indicating whether the toggle indicating third-party cookies are
         *         currently being blocked should be checked or not.
         * @param enforcement A CookieControlsEnforcement enum type indicating the enforcement rule
         *         for these cookie controls.
         */
        void onUpdate(boolean checked, @CookieControlsEnforcement int enforcement);
    }

    private CookieControlsServiceBridge mServiceBridge;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private boolean mIsInitialized;
    private boolean mShowCard;
    private boolean mChecked;
    private @CookieControlsEnforcement int mEnforcement = CookieControlsEnforcement.NO_ENFORCEMENT;

    // State variables for cookie controls at the last UI snapshot
    private boolean mSnapshotChecked;
    private @CookieControlsEnforcement int mSnapshotEnforcement =
            CookieControlsEnforcement.NO_ENFORCEMENT;

    /**
     * Initializes the IncognitoCookieControlsManager explicitly.
     */
    public void initialize() {
        if (mIsInitialized) return;

        mServiceBridge = new CookieControlsServiceBridge(this);
        mShowCard = true;
        mIsInitialized = true;
    }

    /**
     * @param observer An observer to be notified of changes.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return a boolean indicating if the card should be visible or not.
     */
    public boolean shouldShowCookieControlsCard() {
        // TODO(crbug.com/1104836): This is always true. Remove this method and everything that
        // depends on it.
        return mShowCard;
    }

    /**
     * Tells the bridge to update itself if necessary.
     */
    public void updateIfNecessary() {
        if (mShowCard) mServiceBridge.updateServiceIfNecessary();
    }

    /**
     * Tells the caller if the state has changed since the last snaptshot.
     * @return whether a new snapshot should be captured or not.
     */
    protected boolean shouldCaptureThumbnail() {
        boolean changed = mSnapshotEnforcement != mEnforcement || mSnapshotChecked != mChecked;
        mSnapshotChecked = mChecked;
        mSnapshotEnforcement = mEnforcement;
        return changed;
    }

    @Override
    public void sendCookieControlsUIChanges(
            boolean checked, @CookieControlsEnforcement int enforcement) {
        mChecked = checked;
        mEnforcement = enforcement;
        for (Observer obs : mObservers) {
            obs.onUpdate(checked, enforcement);
        }
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (isChecked != mChecked && (buttonView.getId() == R.id.cookie_controls_card_toggle)) {
            mServiceBridge.handleCookieControlsToggleChanged(isChecked);
        }
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.cookie_controls_card_managed_icon) {
            Bundle fragmentArguments = new Bundle();
            fragmentArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                    SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.COOKIES));
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(
                    v.getContext(), SingleCategorySettings.class, fragmentArguments);
        }
    }
}
