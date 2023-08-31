// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed;

import android.content.Context;

import com.google.android.material.color.MaterialColors;

import org.chromium.chrome.browser.xsurface.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * Provides chrome colors to xsurface material color tokens.
 */
public class ColorProviderImpl implements ColorProvider {
    private static final String TAG = "ColorProviderImpl";

    private final Context mContext;

    public ColorProviderImpl(Context context) {
        mContext = context;
    }

    @Override
    public int getPrimary() {
        return MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG);
    }

    @Override
    public int getOnPrimary() {
        return MaterialColors.getColor(mContext, R.attr.colorOnPrimary, TAG);
    }

    @Override
    public int getPrimaryContainer() {
        return MaterialColors.getColor(mContext, R.attr.colorPrimaryContainer, TAG);
    }

    @Override
    public int getOnPrimaryContainer() {
        return MaterialColors.getColor(mContext, R.attr.colorOnPrimaryContainer, TAG);
    }

    @Override
    public int getPrimaryInverse() {
        return MaterialColors.getColor(mContext, R.attr.colorPrimaryInverse, TAG);
    }

    @Override
    public int getSecondary() {
        return MaterialColors.getColor(mContext, R.attr.colorSecondary, TAG);
    }

    @Override
    public int getOnSecondary() {
        return MaterialColors.getColor(mContext, R.attr.colorOnSecondary, TAG);
    }

    @Override
    public int getSecondaryContainer() {
        return MaterialColors.getColor(mContext, R.attr.colorSecondaryContainer, TAG);
    }

    @Override
    public int getOnSecondaryContainer() {
        return MaterialColors.getColor(mContext, R.attr.colorOnSecondaryContainer, TAG);
    }

    @Override
    public int getSurface() {
        return ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0);
    }

    @Override
    public int getOnSurface() {
        return MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG);
    }

    @Override
    public int getSurfaceVariant() {
        return MaterialColors.getColor(mContext, R.attr.colorSurfaceVariant, TAG);
    }

    @Override
    public int getOnSurfaceVariant() {
        return MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, TAG);
    }

    @Override
    public int getSurfaceInverse() {
        return MaterialColors.getColor(mContext, R.attr.colorSurfaceInverse, TAG);
    }

    @Override
    public int getOnSurfaceInverse() {
        return MaterialColors.getColor(mContext, R.attr.colorOnSurfaceInverse, TAG);
    }

    @Override
    public int getError() {
        return MaterialColors.getColor(mContext, R.attr.colorError, TAG);
    }

    @Override
    public int getOnError() {
        return MaterialColors.getColor(mContext, R.attr.colorOnError, TAG);
    }

    @Override
    public int getOutline() {
        return MaterialColors.getColor(mContext, R.attr.colorOutline, TAG);
    }

    @Override
    public int getDivider() {
        return MaterialColors.getColor(mContext, R.attr.colorSurfaceVariant, TAG);
    }
}
