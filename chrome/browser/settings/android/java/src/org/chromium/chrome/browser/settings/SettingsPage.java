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
        /** Initialize settings fragment inside the container. */
        void initSettings(ViewGroup containerView);

        /** Destroy settings fragment. */
        void destroySettings();
    }

    private final String mTitle;
    private final FrameLayout mContentView;
    private final FragmentDelegate mFragmentDelegate;

    /**
     * Create a new instance of the settings page with back press handling.
     *
     * @param activity The current {@link Activity} used to obtain resources or inflate views.
     * @param profile The Profile associated with the settings UI.
     * @param host A NativePageHost to load urls.
     * @param fragmentDelegate The delegate to initialize and destroy settings fragments.
     * @param backPressHandler The back press handler for the settings page.
     * @param backPressHandlerRegistry Back press handler registry to register back press handling.
     */
    public SettingsPage(
            Activity activity,
            Profile profile,
            NativePageHost host,
            FragmentDelegate fragmentDelegate,
            BackPressHandler backPressHandler,
            BackPressHandlerRegistry backPressHandlerRegistry) {
        super(host);

        mTitle = activity.getString(R.string.settings);
        mContentView = new FrameLayout(activity);

        mFragmentDelegate = fragmentDelegate;
        mFragmentDelegate.initSettings(mContentView);

        initWithView(mContentView);

        setBackPressHandler(backPressHandler, backPressHandlerRegistry);
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
    public void destroy() {
        mFragmentDelegate.destroySettings();
        super.destroy();
    }
}
