// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

/** Resolves data by reading from Android resources on demand. */
public class ResourceButtonData implements DisplayButtonData {
    private final @StringRes int mTextRes;
    private final @DrawableRes int mIconRes;

    /** Stores resource ids until resolution time. */
    public ResourceButtonData(@StringRes int textRes, @DrawableRes int iconRes) {
        mTextRes = textRes;
        mIconRes = iconRes;
    }

    @Override
    public String resolveText(Context context) {
        return context.getString(mTextRes);
    }

    @Override
    public Drawable resolveIcon(Context context) {
        return AppCompatResources.getDrawable(context, mIconRes);
    }
}
