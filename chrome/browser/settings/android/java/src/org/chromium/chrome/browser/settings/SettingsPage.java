// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.components.embedder_support.util.UrlConstants;

/** A native page holding the Chrome settings UI in a tab. */
@NullMarked
public class SettingsPage extends BasicNativePage {
    /** Delegate to embed settings fragments into the settings page. */
    public interface FragmentDelegate {
        /**
         * Initialize settings fragment inside the container with an optional initial URL.
         *
         * @param containerView Parent view container.
         * @param initialUrl Initial settings URL (e.g. restored tab URL
         *     "chrome://settings/language"). If an empty string is supplied, the URL resolves to
         *     "chrome://settings".
         */
        void initSettings(ViewGroup containerView, String initialUrl);

        /**
         * Update displayed fragment for a new chrome://settings URL.
         *
         * @param url The new settings URL.
         */
        void updateForUrl(String url);

        /** Destroy settings fragment. */
        void destroySettings();
    }

    private final String mTitle;
    private final FrameLayout mContentView;
    private final FragmentDelegate mFragmentDelegate;

    /**
     * Create a new instance of the settings page with back press handling, and an initial URL.
     *
     * @param activity The current {@link Activity} used to obtain resources or inflate views.
     * @param profile The Profile associated with the settings UI.
     * @param host A NativePageHost to load urls.
     * @param fragmentDelegate The delegate to initialize and destroy settings fragments.
     * @param backPressHandler The back press handler for the settings page.
     * @param backPressHandlerRegistry Back press handler registry to register back press handling.
     * @param url Initial settings URL (e.g. "chrome://settings/language").
     */
    public SettingsPage(
            Activity activity,
            Profile profile,
            NativePageHost host,
            FragmentDelegate fragmentDelegate,
            BackPressHandler backPressHandler,
            BackPressHandlerRegistry backPressHandlerRegistry,
            String url) {
        super(host);

        mTitle = activity.getString(R.string.settings);
        mContentView = new FrameLayout(activity);

        mFragmentDelegate = fragmentDelegate;
        mFragmentDelegate.initSettings(mContentView, url);

        initWithView(mContentView);
        setBackPressHandler(backPressHandler, backPressHandlerRegistry);
        updateForUrl(url);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.SETTINGS_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mFragmentDelegate.updateForUrl(url);
    }

    @Override
    public void destroy() {
        mFragmentDelegate.destroySettings();
        super.destroy();
    }
}
