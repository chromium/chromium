// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * A {@link Preference} customized for the password check button in the password settings menu.
 */
public class PasswordCheckPreference extends Preference {
    private final boolean mShowImage;

    /**
     * Constructor for inflating from XML.
     */
    public PasswordCheckPreference(Context context, boolean showImage) {
        super(context, null);
        setLayoutResource(R.layout.password_check_preference_button);
        mShowImage = showImage;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        holder.findViewById(R.id.password_check_preference_image)
                .setVisibility(mShowImage ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    public ImageView getPromoImageView(Activity activity) {
        return activity.findViewById(R.id.password_check_preference_image);
    }
}
