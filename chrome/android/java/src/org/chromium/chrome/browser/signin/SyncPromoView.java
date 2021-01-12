// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher.AccessPoint;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * A View that shows the user the next step they must complete to start syncing their data (eg.
 * Recent Tabs or Bookmarks).
 * If inflated manually, {@link SyncPromoView#init(int)} must be called before
 * attaching this View to a ViewGroup.
 */
public class SyncPromoView
        extends LinearLayout implements ProfileSyncService.SyncStateChangedListener {
    private @AccessPoint int mAccessPoint;
    private boolean mInitialized;

    private TextView mTitle;
    private TextView mDescription;
    private Button mPositiveButton;

    /**
     * A convenience method to inflate and initialize a SyncPromoView.
     * @param parent A parent used to provide LayoutParams (the SyncPromoView will not be
     *         attached).
     * @param accessPoint Where the SyncPromoView is used.
     */
    public static SyncPromoView create(ViewGroup parent, @AccessPoint int accessPoint) {
        // TODO(injae): crbug.com/829548
        SyncPromoView result = (SyncPromoView) LayoutInflater.from(parent.getContext())
                                       .inflate(R.layout.sync_promo_view, parent, false);
        result.init(accessPoint);
        return result;
    }

    /**
     * Constructor for inflating from xml.
     */
    public SyncPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        // This promo is about enabling sync, so no sense in showing it if
        // syncing isn't possible.
        assert ProfileSyncService.get() != null;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.title);
        mDescription = findViewById(R.id.description);
        mPositiveButton = findViewById(R.id.sign_in);
    }

    /**
     * Provide the information necessary for this class to function.
     * @param accessPoint Where this UI component is used.
     */
    public void init(@AccessPoint int accessPoint) {
        mAccessPoint = accessPoint;
        mInitialized = true;

        assert mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                || mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "SyncPromoView only has strings for bookmark manager and recent tabs.";

        // The title stays the same no matter what action the user must take.
        if (mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            mTitle.setText(R.string.sync_your_bookmarks);
        } else {
            mTitle.setVisibility(View.GONE);
        }

        // We don't call update() here as it will be called in onAttachedToWindow().
    }

    private void update() {
        ViewState viewState;
        if (!ProfileSyncService.get().isSyncAllowedByPlatform()) {
            viewState = getStateForEnableAndroidSync();
        } else if (!ProfileSyncService.get().isSyncRequested()) {
            viewState = getStateForEnableChromeSync();
        } else {
            viewState = getStateForStartUsing();
        }
        viewState.apply(mDescription, mPositiveButton);
    }

    /**
     * The ViewState class represents all the UI elements that can change for each variation of
     * this View. We use this to ensure each variation (created in the getStateFor* methods)
     * explicitly touches each UI element.
     */
    private static class ViewState {
        private final int mDescriptionText;
        private final ButtonState mPositiveButtonState;

        public ViewState(int mDescriptionText, ButtonState mPositiveButtonState) {
            this.mDescriptionText = mDescriptionText;
            this.mPositiveButtonState = mPositiveButtonState;
        }

        public void apply(TextView description, Button positiveButton) {
            description.setText(mDescriptionText);
            mPositiveButtonState.apply(positiveButton);
        }
    }

    /**
     * Classes to represent the state of a button that we are interested in, used to keep ViewState
     * tidy and provide some convenience methods.
     */
    private interface ButtonState {
        void apply(Button button);
    }

    private static class ButtonAbsent implements ButtonState {
        @Override
        public void apply(Button button) {
            button.setVisibility(View.GONE);
        }
    }

    private static class ButtonPresent implements ButtonState {
        private final int mTextResource;
        private final OnClickListener mOnClickListener;

        // TODO(crbug.com/1107904): Once Chrome is decoupled from auto-sync,
        // |onClickListener| can be inlined.
        public ButtonPresent(int textResource, OnClickListener onClickListener) {
            mTextResource = textResource;
            mOnClickListener = onClickListener;
        }

        @Override
        public void apply(Button button) {
            button.setVisibility(View.VISIBLE);
            button.setText(mTextResource);
            button.setOnClickListener(mOnClickListener);
        }
    }

    private ViewState getStateForEnableAndroidSync() {
        assert mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "Enable Android Sync should not be showing from bookmarks";

        int descId = R.string.recent_tabs_sync_promo_enable_android_sync;

        ButtonState positiveButton = new ButtonPresent(R.string.open_settings_button, view -> {
            IntentUtils.safeStartActivity(getContext(), new Intent(Settings.ACTION_SYNC_SETTINGS));
        });

        return new ViewState(descId, positiveButton);
    }

    private ViewState getStateForEnableChromeSync() {
        int descId = mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                ? R.string.bookmarks_sync_promo_enable_sync
                : R.string.recent_tabs_sync_promo_enable_chrome_sync;

        ButtonState positiveButton = new ButtonPresent(R.string.enable_sync_button, view -> {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(getContext(), SyncAndServicesSettings.class,
                    SyncAndServicesSettings.createArguments(false));
        });

        return new ViewState(descId, positiveButton);
    }

    private ViewState getStateForStartUsing() {
        // TODO(peconn): Ensure this state is never seen when used for bookmarks.
        // State is updated before this view is removed, so this invalid state happens, but is not
        // visible. I want there to be a guarantee that this state is never seen, but to do so would
        // require some code restructuring.

        return new ViewState(R.string.ntp_recent_tabs_sync_promo_instructions, new ButtonAbsent());
    }

    @Override
    protected void onAttachedToWindow() {
        assert mInitialized : "init(...) must be called on SyncPromoView before use.";

        super.onAttachedToWindow();
        ProfileSyncService.get().addSyncStateChangedListener(this);
        update();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        ProfileSyncService.get().removeSyncStateChangedListener(this);
    }

    // ProfileSyncService.SyncStateChangedListener
    @Override
    public void syncStateChanged() {
        update();
    }
}
