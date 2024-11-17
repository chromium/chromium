// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.MaterialCardViewNoShadow;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.DeviceFormFactor;

// TODO(crbug.com/40223169): Extend the comment below to explain under which circumstances this
// class is
// still used
/**
 * A View that shows the user the next step they must complete to start syncing their data (eg.
 * Recent Tabs or Bookmarks). If inflated manually, either {@link LegacySyncPromoView#init(int)} or
 * {@link LegacySyncPromoView#setInitializeNotRequired()} must be called before attaching this View
 * to a ViewGroup.
 */
public class LegacySyncPromoView extends FrameLayout
        implements SyncService.SyncStateChangedListener {
    private SyncService mSyncService;
    private @AccessPoint int mAccessPoint;
    private boolean mInitialized;
    private boolean mInitializeNotRequired;

    private TextView mTitle;
    private TextView mDescription;
    private View mEmptyView;
    private TextView mEmptyStateTitle;
    private TextView mEmptyStateDescription;
    private ImageView mEmptyStateImage;
    private Button mPositiveButton;
    private MaterialCardViewNoShadow mOldEmptyCardView;

    /**
     * A convenience method to inflate and initialize a LegacySyncPromoView.
     *
     * @param parent A parent used to provide LayoutParams (the LegacySyncPromoView will not be
     *     attached).
     * @param profile The {@link Profile} associated with the sync promotion.
     * @param accessPoint Where the LegacySyncPromoView is used.
     */
    public static LegacySyncPromoView create(
            ViewGroup parent, Profile profile, @AccessPoint int accessPoint) {
        // TODO(injae): crbug.com/829548
        LegacySyncPromoView result =
                (LegacySyncPromoView)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.legacy_sync_promo_view, parent, false);
        result.init(profile, accessPoint);
        return result;
    }

    /** Constructor for inflating from xml. */
    public LegacySyncPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);

    }

    @Override
    @SuppressWarnings("WrongViewCast") // False positive.
    protected void onFinishInflate() {
        super.onFinishInflate();

        // @Todo(crbug.com/1465523) Refactor Recent Tabs Empty States implementation. We don't want
        // this implementation to live in this class and we can add another subclass of PromoGroup
        // similar to the one used for LegacySyncPromoView.
        ViewStub emptyViewStub = findViewById(R.id.recent_tab_empty_state_view_stub);
        mEmptyView = emptyViewStub.inflate();
        mEmptyStateTitle = findViewById(R.id.empty_state_text_title);
        mEmptyStateDescription = findViewById(R.id.empty_state_text_description);
        mEmptyStateImage = findViewById(R.id.empty_state_icon);
        mOldEmptyCardView = findViewById(R.id.card_view);
        mTitle = findViewById(R.id.title);
        mDescription = findViewById(R.id.description);
        mPositiveButton = findViewById(R.id.sign_in);
    }

    public TextView getEmptyStateTitle() {
        return mEmptyStateTitle;
    }

    public TextView getEmptyStateDescription() {
        return mEmptyStateDescription;
    }

    public ImageView getEmptyStateImage() {
        return mEmptyStateImage;
    }

    public MaterialCardViewNoShadow getOldEmptyCardView() {
        return mOldEmptyCardView;
    }

    public View getEmptyStateView() {
        return mEmptyView;
    }

    /**
     * Provide the information necessary for this class to function.
     *
     * @param profile The {@link Profile} associated with the sync promotion.
     * @param accessPoint Where this UI component is used.
     */
    public void init(Profile profile, @AccessPoint int accessPoint) {
        mSyncService = SyncServiceFactory.getForProfile(profile);
        // This promo is about enabling sync, so no sense in showing it if
        // syncing isn't possible.
        assert mSyncService != null;

        mAccessPoint = accessPoint;
        mInitialized = true;

        assert mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                        || mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "LegacySyncPromoView only has strings for bookmark manager and recent tabs.";

        // The title stays the same no matter what action the user must take.
        if (mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            mTitle.setText(R.string.sync_your_bookmarks);
        } else {
            mTitle.setVisibility(View.GONE);
        }

        // We don't call update() here as it will be called in onAttachedToWindow().
    }

    public void setInitializeNotRequired() {
        mInitializeNotRequired = true;
    }

    private void update() {
        ViewState viewState;
        if (!mSyncService.hasSyncConsent() || mSyncService.getSelectedTypes().isEmpty()) {
            viewState = getStateForEnableChromeSync();
            viewState.apply(mDescription, mPositiveButton, mEmptyView, mOldEmptyCardView);
        } else {
            viewState = getStateForStartUsing();
            if (mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
                viewState.apply(mDescription, mPositiveButton, mEmptyView, mOldEmptyCardView);
            } else {
                viewState.applyEmptyView(
                        mEmptyStateTitle,
                        mEmptyStateDescription,
                        mEmptyStateImage,
                        mOldEmptyCardView,
                        mEmptyView);
            }
        }
    }

    /**
     * The ViewState class represents all the UI elements that can change for each variation of
     * this View. We use this to ensure each variation (created in the getStateFor* methods)
     * explicitly touches each UI element.
     */
    private static class ViewState {
        private int mDescriptionText;
        private ButtonState mPositiveButtonState;
        private int mEmptyStateTitleText;
        private int mEmptyStateDescriptionText;
        private int mEmptyStateImageResource;

        public ViewState(int mDescriptionText, ButtonState mPositiveButtonState) {
            this.mDescriptionText = mDescriptionText;
            this.mPositiveButtonState = mPositiveButtonState;
        }

        // Initialize empty State view resources.
        public ViewState(
                int mEmptyStateTitleText,
                int mEmptyStateDescriptionText,
                int mEmptyStateImageResource) {
            this.mEmptyStateTitleText = mEmptyStateTitleText;
            this.mEmptyStateDescriptionText = mEmptyStateDescriptionText;
            this.mEmptyStateImageResource = mEmptyStateImageResource;
        }

        // Apply empty state view resources.
        public void applyEmptyView(
                TextView emptyStateTitle,
                TextView emptyStateDescription,
                ImageView emptyStateImageView,
                MaterialCardViewNoShadow oldEmptyCardView,
                View emptyStateView) {
            emptyStateTitle.setText(mEmptyStateTitleText);
            emptyStateDescription.setText(mEmptyStateDescriptionText);
            emptyStateImageView.setImageResource(mEmptyStateImageResource);
            oldEmptyCardView.setVisibility(View.GONE);
            emptyStateView.setVisibility(View.VISIBLE);
        }

        public void apply(
                TextView description,
                Button positiveButton,
                View emptyStateView,
                MaterialCardViewNoShadow oldEmptyCardView) {
            description.setText(mDescriptionText);
            mPositiveButtonState.apply(positiveButton);
            oldEmptyCardView.setVisibility(View.VISIBLE);
            if (emptyStateView != null) {
                emptyStateView.setVisibility(View.GONE);
            }
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

        // TODO(crbug.com/40141050): Once Chrome is decoupled from auto-sync,
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
        int descId =
                mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                        ? R.string.bookmarks_sync_promo_enable_sync
                        : R.string.recent_tabs_sync_promo_enable_chrome_sync;

        ButtonState positiveButton =
                new ButtonPresent(
                        R.string.enable_sync_button,
                        view -> {
                            SettingsNavigation settingsNavigation =
                                    SettingsNavigationFactory.createSettingsNavigation();
                            settingsNavigation.startSettings(
                                    getContext(),
                                    ManageSyncSettings.class,
                                    ManageSyncSettings.createArguments(false));
                        });

        return new ViewState(descId, positiveButton);
    }

    private ViewState getStateForStartUsing() {
        // TODO(peconn): Ensure this state is never seen when used for bookmarks.
        // State is updated before this view is removed, so this invalid state happens, but is not
        // visible. I want there to be a guarantee that this state is never seen, but to do so would
        // require some code restructuring.
        if (mAccessPoint != SigninAccessPoint.BOOKMARK_MANAGER) {
            int emptyViewImageResId =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                            ? R.drawable.tablet_recent_tab_empty_state_illustration
                            : R.drawable.phone_recent_tab_empty_state_illustration;
            return new ViewState(
                    R.string.recent_tabs_no_tabs_empty_state,
                    R.string.recent_tabs_sign_in_on_other_devices,
                    emptyViewImageResId);
        } else {
            return new ViewState(
                    R.string.ntp_recent_tabs_sync_promo_instructions, new ButtonAbsent());
        }
    }

    @Override
    protected void onAttachedToWindow() {
        if (!mInitializeNotRequired) {
            assert mInitialized : "init(...) must be called on LegacySyncPromoView before use.";
        } else {
            assert !mInitialized : "init is not required.";
        }

        super.onAttachedToWindow();
        if (mInitialized) {
            mSyncService.addSyncStateChangedListener(this);
            update();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        if (!mInitializeNotRequired) {
            assert mInitialized : "init(...) must be called on LegacySyncPromoView before use.";
        } else {
            assert !mInitialized : "init is not required.";
        }

        super.onDetachedFromWindow();
        if (mInitialized) {
            mSyncService.removeSyncStateChangedListener(this);
        }
    }

    // SyncService.SyncStateChangedListener
    @Override
    public void syncStateChanged() {
        update();
    }
}
