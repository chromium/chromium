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

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.UiUtils;

import java.util.HashMap;
import java.util.Set;

public class BatchUploadCardPreference extends Preference
        implements SyncService.SyncStateChangedListener {
    private Profile mProfile;
    private SyncService mSyncService;
    private HashMap<Integer, LocalDataDescription> mLocalDataDescriptionsMap;

    public BatchUploadCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signin_settings_card_view);
    }

    /** Initialize the dependencies for the BatchUploadCardPreference and update the error card. */
    public void initialize(Profile profile) {
        mProfile = profile;
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
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

        holder.setDividerAllowedAbove(false);
        setupBatchUploadCardView(holder.findViewById(R.id.signin_settings_card));
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        update();
    }

    private void update() {
        mSyncService.getLocalDataDescriptions(
                Set.of(ModelType.BOOKMARKS, ModelType.PASSWORDS, ModelType.READING_LIST),
                localDataDescriptionsMap -> {
                    mLocalDataDescriptionsMap = localDataDescriptionsMap;
                    int sum =
                            mLocalDataDescriptionsMap.values().stream()
                                    .map(LocalDataDescription::itemCount)
                                    .reduce(0, Integer::sum);
                    if (sum == 0) {
                        setVisible(false);
                    } else {
                        setVisible(true);
                        notifyChanged();
                    }
                });
    }

    private void setupBatchUploadCardView(View card) {
        if (mLocalDataDescriptionsMap == null) {
            return;
        }

        Context context = getContext();

        Button button = (Button) card.findViewById(R.id.signin_settings_card_button);
        button.setText(R.string.account_settings_bulk_upload_section_save_button);

        // TODO(b/326040498): Remove the stub usages of the strings below and create a dialog here
        // where those strings will be used.
        stubUsage(
                R.string.account_settings_bulk_upload_dialog_title,
                R.string.account_settings_bulk_upload_dialog_save_button,
                R.string.account_settings_bulk_upload_dialog_cancel_button,
                R.string.account_settings_bulk_upload_dialog_description,
                context.getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_dialog_bookmarks, 1, 1),
                context.getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_dialog_passwords, 1, 1),
                context.getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_dialog_reading_list, 1, 1),
                context.getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_saved_snackbar_message,
                                1,
                                "elisa.g.beckett@gmail.com"));

        ImageView image = (ImageView) card.findViewById(R.id.signin_settings_card_icon);
        image.setImageDrawable(
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.ic_cloud_upload_24dp,
                        R.color.default_icon_color_accent1_tint_list));

        int localPasswordsCount = mLocalDataDescriptionsMap.get(ModelType.PASSWORDS).itemCount();
        int localItemsCount =
                mLocalDataDescriptionsMap.get(ModelType.BOOKMARKS).itemCount()
                        + mLocalDataDescriptionsMap.get(ModelType.READING_LIST).itemCount();

        TextView text = (TextView) card.findViewById(R.id.signin_settings_card_description);
        // TODO(b/354686035): Handle accounts with non-displayable email address.
        CoreAccountInfo accountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfile)
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (localItemsCount == 0) {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_password,
                                    localPasswordsCount,
                                    localPasswordsCount,
                                    accountInfo.getEmail()));
        } else if (localPasswordsCount == 0) {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_other,
                                    localItemsCount,
                                    localItemsCount,
                                    accountInfo.getEmail()));
        } else {
            text.setText(
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .account_settings_bulk_upload_section_description_password_and_other,
                                    localPasswordsCount,
                                    localPasswordsCount,
                                    accountInfo.getEmail()));
        }
    }

    // TODO(b/326040498): This method should be removed.
    private void stubUsage(
            int s1, int s2, int s3, int s4, String s5, String s6, String s7, String s8) {}
}
