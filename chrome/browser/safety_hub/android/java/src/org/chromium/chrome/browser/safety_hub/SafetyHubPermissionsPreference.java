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
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

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
        // TODO(crbug.com/324562205): Display correct substring based on revoked
        // permissions.

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
}
