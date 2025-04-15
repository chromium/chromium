// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.stream.IntStream;

class SafetyHubPermissionsPreference extends ChromeBasePreference implements View.OnClickListener {
    private final @NonNull PermissionsData mPermissionsData;
    private final @NonNull LargeIconBridge mLargeIconBridge;
    private boolean mFaviconFetched;

    SafetyHubPermissionsPreference(
            Context context,
            @NonNull PermissionsData permissionsData,
            @NonNull LargeIconBridge largeIconBridge) {
        super(context);

        mPermissionsData = permissionsData;
        mLargeIconBridge = largeIconBridge;
        setTitle(mPermissionsData.getOrigin());
        setSummary(createSummary());

        setSelectable(false);
        setDividerAllowedBelow(false);
        setWidgetLayoutResource(R.layout.safety_hub_button_widget);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat button = (ButtonCompat) holder.findViewById(R.id.button);
        button.setText(R.string.undo);
        button.setOnClickListener(this);

        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched) {
            FaviconLoader.loadFavicon(
                    getContext(),
                    mLargeIconBridge,
                    new GURL(mPermissionsData.getOrigin()),
                    this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    @Override
    public void onClick(View v) {
        if (getOnPreferenceClickListener() != null) {
            getOnPreferenceClickListener().onPreferenceClick(this);
        }
    }

    @NonNull
    PermissionsData getPermissionsData() {
        return mPermissionsData;
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }

    private String createSummary() {
        if (IntStream.of(mPermissionsData.getPermissions())
                .anyMatch(x -> x == ContentSettingsType.NOTIFICATIONS)) {
            return getContext()
                    .getString(R.string.safety_hub_abusive_notification_permissions_sublabel);
        }

        String[] permissionNames =
                UnusedSitePermissionsBridge.contentSettingsTypeToString(
                        mPermissionsData.getPermissions());
        assert permissionNames.length > 0 : "Site does not have revoked permissions.";

        switch (permissionNames.length) {
            case 1:
                return getContext()
                        .getString(
                                R.string.safety_hub_removed_one_permission_sublabel,
                                permissionNames[0]);
            case 2:
                return getContext()
                        .getString(
                                R.string.safety_hub_removed_two_permissions_sublabel,
                                permissionNames[0],
                                permissionNames[1]);
            case 3:
                return getContext()
                        .getString(
                                R.string.safety_hub_removed_three_permissions_sublabel,
                                permissionNames[0],
                                permissionNames[1],
                                permissionNames[2]);
            default:
                return getContext()
                        .getString(
                                R.string.safety_hub_removed_four_or_more_permissions_sublabel,
                                permissionNames[0],
                                permissionNames[1],
                                permissionNames.length - 2);
        }
    }
}
