// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;
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

        ImageView image = (ImageView) holder.findViewById(R.id.central_account_image);
        image.setImageDrawable(profileData.getImage());

        TextView name = (TextView) holder.findViewById(R.id.central_account_name);
        TextView email = (TextView) holder.findViewById(R.id.central_account_email);
        if (profileData.getFullName() != null) {
            email.setTextAppearance(R.style.TextAppearance_TextSmall_Secondary);
            name.setVisibility(View.VISIBLE);
            name.setText(profileData.getFullName());
        } else {
            // TODO(crbug.com/345687670): Add render test for this case.
            name.setVisibility(View.GONE);
            email.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
        }
        email.setText(profileData.getAccountEmail());
    }

    /** ProfileDataCache.Observer implementation. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        notifyChanged();
    }
}
