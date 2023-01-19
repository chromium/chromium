// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;

// TODO(crbug/1319351): Extend the comment below to explain under which circumstances this class is
// still used
/**
 * A View that shows the user the next step they must complete to start syncing their data (eg.
 * Recent Tabs or Bookmarks).
 * If inflated manually, {@link LegacySyncPromoView#init(int)} must be called before
 * attaching this View to a ViewGroup.
 */
public class LegacySyncPromoView
        extends LinearLayout implements SyncService.SyncStateChangedListener {
    private @AccessPoint int mAccessPoint;
    private boolean mInitialized;

    private TextView mTitle;
    private TextView mDescription;
    private Button mPositiveButton;

    /**
     * A convenience method to inflate and initialize a LegacySyncPromoView.
     * @param parent A parent used to provide LayoutParams (the LegacySyncPromoView will not be
     *         attached).
     * @param accessPoint Where the LegacySyncPromoView is used.
     */
    public static LegacySyncPromoView create(ViewGroup parent, @AccessPoint int accessPoint) {
        // TODO(injae): crbug.com/829548
        LegacySyncPromoView result =
                (LegacySyncPromoView) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.legacy_sync_promo_view, parent, false);
        result.init(accessPoint);
        return result;
    }

    /**
     * Constructor for inflating from xml.
     */
    public LegacySyncPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        // This promo is about enabling sync, so no sense in showing it if
        // syncing isn't possible.
        assert SyncService.get() != null;
    }

    @Override
    @SuppressWarnings("WrongViewCast") // False positive.
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
                || mAccessPoint
                        == SigninAccessPoint.RECENT_TABS
            : "LegacySyncPromoView only has strings for bookmark manager and recent tabs.";

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
        if (!SyncService.get().isSyncRequested()
                || SyncService.get().getSelectedTypes().isEmpty()) {
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

    private ViewState getStateForEnableChromeSync() {
        int descId = mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                ? R.string.bookmarks_sync_promo_enable_sync
                : R.string.recent_tabs_sync_promo_enable_chrome_sync;

        ButtonState positiveButton = new ButtonPresent(R.string.enable_sync_button, view -> {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(getContext(), ManageSyncSettings.class,
                    ManageSyncSettings.createArguments(false));
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
        assert mInitialized : "init(...) must be called on LegacySyncPromoView before use.";

        super.onAttachedToWindow();
        SyncService.get().addSyncStateChangedListener(this);
        update();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        SyncService.get().removeSyncStateChangedListener(this);
    }

    // SyncService.SyncStateChangedListener
    @Override
    public void syncStateChanged() {
        update();
    }
}
