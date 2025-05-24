// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;

import androidx.preference.DialogPreference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.RecordType;

import java.util.OptionalInt;

/** Launches the UI to edit, create or delete an Autofill profile entry. */
@NullMarked
public class AutofillProfileEditorPreference extends DialogPreference {

    private boolean mShouldShowLocalProfileIcon;
    private OptionalInt mRecordType = OptionalInt.empty();

    public AutofillProfileEditorPreference(Context context) {
        super(context);
    }

    /**
     * @return ID of the profile to edit when this preference is selected.
     */
    public @Nullable String getGUID() {
        return getExtras().getString(AutofillEditorBase.AUTOFILL_GUID);
    }

    /**
     * @return True if profile is only local and special icon should be shown.
     */
    public boolean shouldShowLocalProfileIcon() {
        return mShouldShowLocalProfileIcon;
    }

    /**
     * Sets whether to display the "local profile" icon.
     *
     * @param shouldShowLocalProfileIcon {@code true} to show the icon, {@code false} to hide it.
     * @see #shouldShowLocalProfileIcon()
     */
    public void setShouldShowLocalProfileIcon(boolean shouldShowLocalProfileIcon) {
        mShouldShowLocalProfileIcon = shouldShowLocalProfileIcon;
    }

    /**
     * @return RecordType of the autofill profile (home, work, etc.).
     */
    public OptionalInt getRecordType() {
        return mRecordType;
    }

    /**
     * Sets the {@link RecordType} of this autofill profile (e.g., home, work).
     *
     * @param recordType The new {@link RecordType}.
     */
    public void setRecordType(@RecordType int recordType) {
        mRecordType = OptionalInt.of(recordType);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        if (!getRecordType().isPresent()) {
            return;
        }

        ImageView localProfileIconView = (ImageView) holder.findViewById(R.id.local_profile_icon);
        ImageView editAddressIconView = (ImageView) holder.findViewById(R.id.edit_address_icon);
        // Shows "local profile" icon only for address profiles that are neither synced, nor
        // saved in the account.
        if (!shouldShowLocalProfileIcon()) {
            localProfileIconView.setVisibility(View.GONE);
        }

        // Home & Work profiles have different icon, because clicking on them forwards user to
        // the external service.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK)) {
            if (getRecordType().getAsInt() == RecordType.ACCOUNT_HOME
                    || getRecordType().getAsInt() == RecordType.ACCOUNT_WORK) {
                editAddressIconView.setImageResource(R.drawable.autofill_external_link);
            } else {
                editAddressIconView.setImageResource(R.drawable.autofill_chevron_right);
            }
        } else {
            editAddressIconView.setVisibility(View.GONE);
        }
    }
}
