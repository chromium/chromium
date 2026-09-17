// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.SyncSettingsUtils.ErrorUiAction;
import org.chromium.chrome.browser.sync.ui.IdentityErrorCardViewBinder;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;

@NullMarked
public class IdentityErrorCardPreference extends ChromeBasePreference
        implements SyncService.SyncStateChangedListener {
    public interface Listener {
        /** Called when the user clicks the button. */
        void onIdentityErrorCardButtonClicked(@UserActionableError int error);

        /** Called when the visibility of the error card changes. */
        default void onIdentityErrorCardVisibilityChanged() {}
    }

    private @Nullable Profile mProfile;
    private @Nullable SyncService mSyncService;
    private Listener mListener;

    private @UserActionableError int mIdentityError;

    public IdentityErrorCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signin_settings_card_view);
        mIdentityError = UserActionableError.NONE;
    }

    @Override
    public int getCustomBackgroundStyle() {
        return BackgroundStyle.NONE;
    }

    /**
     * Initialize the dependencies for the IdentityErrorCardPreference and update the error card.
     */
    @Initializer
    public void initialize(Profile profile, Listener listener) {
        assert getParent() != null : "Not attached to any parent.";

        mProfile = profile;
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        mListener = listener;

        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mIdentityError == UserActionableError.NONE) {
            return;
        }
        holder.setDividerAllowedAbove(false);
        IdentityErrorCardViewBinder.bind(
                getContext(),
                holder.findViewById(R.id.signin_settings_card),
                mIdentityError,
                () -> mListener.onIdentityErrorCardButtonClicked(mIdentityError));
    }

    private void update() {
        @UserActionableError int error = SyncSettingsUtils.getSyncError(mProfile);
        if (error == mIdentityError) {
            // Nothing changed.
            return;
        }
        mIdentityError = error;
        if (shouldShowErrorCard()) {
            if (!isVisible()) {
                setVisible(true);
                mListener.onIdentityErrorCardVisibilityChanged();
            }
            notifyChanged();
            RecordHistogram.recordEnumeratedHistogram(
                    "Sync.IdentityErrorCard"
                            + SyncSettingsUtils.getHistogramSuffixForError(mIdentityError),
                    ErrorUiAction.SHOWN,
                    ErrorUiAction.NUM_ENTRIES);
        } else {
            if (isVisible()) {
                setVisible(false);
                mListener.onIdentityErrorCardVisibilityChanged();
            }
        }
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        update();
    }

    private boolean shouldShowErrorCard() {
        return mIdentityError != UserActionableError.NONE;
    }
}
