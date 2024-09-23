// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.signin.base.CoreAccountInfo;

/** A dedicated preference for the account settings top avatar. */
public class CentralAccountCardPreference extends Preference implements ProfileDataCache.Observer {
    private CoreAccountInfo mAccountInfo;
    private ProfileDataCache mProfileDataCache;

    public CentralAccountCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.central_account_card_view);
    }

    /**
     * Initialize the dependencies for the CentralAccountCardPreference.
     *
     * <p>Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    public void initialize(CoreAccountInfo accountInfo, ProfileDataCache profileDataCache) {
        mAccountInfo = accountInfo;
        mProfileDataCache = profileDataCache;
    }

    @Override
    public void onAttached() {
        super.onAttached();

        mProfileDataCache.addObserver(this);
    }

    @Override
    public void onDetached() {
        super.onDetached();

        mProfileDataCache.removeObserver(this);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mAccountInfo.getEmail());

        ImageView imageView = (ImageView) holder.findViewById(R.id.central_account_image);
        imageView.setImageDrawable(profileData.getImage());

        Pair<String, String> primaryAndSecondaryText = getPrimaryAndSecondaryText(profileData);

        TextView primaryText = (TextView) holder.findViewById(R.id.central_account_primary_text);
        primaryText.setText(primaryAndSecondaryText.first);

        TextView secondaryText =
                (TextView) holder.findViewById(R.id.central_account_secondary_text);
        if (!primaryAndSecondaryText.second.isEmpty()) {
            secondaryText.setText(primaryAndSecondaryText.second);
            secondaryText.setVisibility(View.VISIBLE);
        } else {
            secondaryText.setVisibility(View.GONE);
        }
    }

    /** ProfileDataCache.Observer implementation. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        notifyChanged();
    }

    private Pair<String, String> getPrimaryAndSecondaryText(DisplayableProfileData profileData) {
        if (!TextUtils.isEmpty(profileData.getFullName())
                && profileData.hasDisplayableEmailAddress()) {
            return new Pair<>(profileData.getFullName(), profileData.getAccountEmail());
        } else if (!TextUtils.isEmpty(profileData.getFullName())) {
            return new Pair<>(profileData.getFullName(), "");
        } else if (profileData.hasDisplayableEmailAddress()) {
            return new Pair<>(profileData.getAccountEmail(), "");
        }
        // When email and full name cannot be shown, use the default account string instead.
        return new Pair<>(getContext().getString(R.string.default_google_account_username), "");
    }
}
