// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.ErrorUiAction;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

public class SyncErrorCardPreference extends Preference
        implements SyncService.SyncStateChangedListener, ProfileDataCache.Observer {
    /** Listener for the buttons in the error card. */
    public interface SyncErrorCardPreferenceListener {
        /**
         * Called to check if the preference should be hidden in case its created from signin
         * screen.
         */
        boolean shouldSuppressSyncSetupIncomplete();

        /** Called when the user clicks the primary button. */
        void onSyncErrorCardPrimaryButtonClicked();

        /**
         * Called when the user clicks the secondary button. This button is only shown for
         * {@link SyncError.SYNC_SETUP_INCOMPLETE} error.
         */
        void onSyncErrorCardSecondaryButtonClicked();
    }

    private ProfileDataCache mProfileDataCache;
    private Profile mProfile;
    private SyncService mSyncService;
    private IdentityManager mIdentityManager;
    private SyncErrorCardPreferenceListener mListener;
    private @SyncError int mSyncError;

    public SyncErrorCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.sync_promo_view_settings);
        mSyncError = SyncError.NO_ERROR;
        setVisible(false);
    }

    /**
     * Initialize the dependencies for the SyncErrorCardPreference.
     *
     * <p>Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    public void initialize(
            ProfileDataCache profileDataCache,
            Profile profile,
            SyncErrorCardPreferenceListener listener) {
        mProfileDataCache = profileDataCache;
        mProfile = profile;
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(mProfile);
        mListener = listener;
    }

    @Override
    public void onAttached() {
        super.onAttached();
        mProfileDataCache.addObserver(this);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();
        mProfileDataCache.removeObserver(this);
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mSyncError == SyncError.NO_ERROR) {
            return;
        }

        PersonalizedSigninPromoView errorCardView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        setupSyncErrorCardView(errorCardView);
    }

    private void update() {
        if (mSyncError == SyncSettingsUtils.getSyncError(mProfile)) {
            return;
        }

        mSyncError = SyncSettingsUtils.getSyncError(mProfile);
        boolean suppressSyncSetupIncompleteFromSigninPage =
                (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE)
                        && mListener.shouldSuppressSyncSetupIncomplete();
        if (mSyncError == SyncError.NO_ERROR || suppressSyncSetupIncompleteFromSigninPage) {
            setVisible(false);
        } else {
            setVisible(true);
            notifyChanged();
            RecordHistogram.recordEnumeratedHistogram(
                    "Sync.SyncErrorCard" + SyncSettingsUtils.getHistogramSuffixForError(mSyncError),
                    ErrorUiAction.SHOWN,
                    ErrorUiAction.NUM_ENTRIES);
        }
    }

    private void setupSyncErrorCardView(PersonalizedSigninPromoView errorCardView) {
        String signedInAccount =
                CoreAccountInfo.getEmailFrom(
                        mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
        // May happen if account is removed from the device while this screen is shown.
        // ManageSyncSettings will take care of finishing the activity in such case.
        if (signedInAccount == null) {
            return;
        }
        Drawable accountImage =
                mProfileDataCache.getProfileDataOrDefault(signedInAccount).getImage();
        errorCardView.getImage().setImageDrawable(accountImage);

        errorCardView.getDismissButton().setVisibility(View.GONE);
        if (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE) {
            errorCardView.getTitle().setVisibility(View.GONE);
        } else {
            errorCardView.getTitle().setVisibility(View.VISIBLE);
        }
        errorCardView
                .getTitle()
                .setText(SyncSettingsUtils.getSyncErrorCardTitle(getContext(), mSyncError));

        errorCardView
                .getDescription()
                .setText(SyncSettingsUtils.getSyncErrorHint(getContext(), mSyncError));

        errorCardView
                .getPrimaryButton()
                .setText(SyncSettingsUtils.getSyncErrorCardButtonLabel(getContext(), mSyncError));
        errorCardView
                .getPrimaryButton()
                .setOnClickListener(
                        v -> {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "Sync.SyncErrorCard"
                                            + SyncSettingsUtils.getHistogramSuffixForError(
                                                    mSyncError),
                                    ErrorUiAction.BUTTON_CLICKED,
                                    ErrorUiAction.NUM_ENTRIES);
                            mListener.onSyncErrorCardPrimaryButtonClicked();
                        });
        if (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE) {
            errorCardView
                    .getSecondaryButton()
                    .setOnClickListener(v -> mListener.onSyncErrorCardSecondaryButtonClicked());
            errorCardView.getSecondaryButton().setText(R.string.cancel);
        } else {
            errorCardView.getSecondaryButton().setVisibility(View.GONE);
        }
    }

    public @SyncError int getSyncError() {
        return mSyncError;
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        update();
    }

    /** {@link ProfileDataCache.Observer} implementation. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        update();
    }
}
