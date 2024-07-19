// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.ErrorCardDetails;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.ErrorUiAction;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.sync.SyncService;

public class IdentityErrorCardPreference extends Preference
        implements SyncService.SyncStateChangedListener {
    public interface Listener {
        /** Called when the user clicks the button. */
        void onIdentityErrorCardButtonClicked(@SyncError int error);
    }

    private Profile mProfile;
    private SyncService mSyncService;
    private Listener mListener;

    private @SyncError int mIdentityError;

    public IdentityErrorCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signin_settings_card_view);
        mIdentityError = SyncError.NO_ERROR;
    }

    /**
     * Initialize the dependencies for the IdentityErrorCardPreference and update the error card.
     */
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

        if (mIdentityError == SyncError.NO_ERROR) {
            return;
        }
        holder.setDividerAllowedAbove(false);
        setupIdentityErrorCardView(holder.findViewById(R.id.signin_settings_card));
    }

    private void update() {
        @SyncError int error = SyncSettingsUtils.getIdentityError(mProfile);
        if (error == mIdentityError) {
            // Nothing changed.
            return;
        }
        mIdentityError = error;
        if (shouldShowErrorCard()) {
            setVisible(true);
            notifyChanged();
            RecordHistogram.recordEnumeratedHistogram(
                    "Sync.IdentityErrorCard"
                            + SyncSettingsUtils.getHistogramSuffixForError(mIdentityError),
                    ErrorUiAction.SHOWN,
                    ErrorUiAction.NUM_ENTRIES);
        } else {
            setVisible(false);
        }
    }

    private void setupIdentityErrorCardView(View card) {
        Context context = getContext();

        ImageView image = (ImageView) card.findViewById(R.id.signin_settings_card_icon);
        image.setContentDescription(
                context.getResources()
                        .getString(R.string.accessibility_account_management_row_account_error));
        image.setImageDrawable(AppCompatResources.getDrawable(context, R.drawable.ic_error));

        TextView error = card.findViewById(R.id.signin_settings_card_description);
        Button button = card.findViewById(R.id.signin_settings_card_button);

        ErrorCardDetails error_card_details =
                SyncSettingsUtils.getIdentityErrorErrorCardDetails(mIdentityError);
        error.setText(context.getString(error_card_details.message));
        button.setText(context.getString(error_card_details.buttonLabel));

        button.setOnClickListener(
                v -> {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Sync.IdentityErrorCard"
                                    + SyncSettingsUtils.getHistogramSuffixForError(mIdentityError),
                            ErrorUiAction.BUTTON_CLICKED,
                            ErrorUiAction.NUM_ENTRIES);
                    mListener.onIdentityErrorCardButtonClicked(mIdentityError);
                });
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        update();
    }

    private boolean shouldShowErrorCard() {
        return mIdentityError != SyncError.NO_ERROR;
    }
}
