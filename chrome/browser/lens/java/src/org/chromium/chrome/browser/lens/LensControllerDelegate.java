// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.content.Intent;
import android.net.Uri;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * Base class for defining methods where different behavior is required by downstream targets. The
 * correct implementation of {@link LensControllerDelegate} will be determined at compile time via
 * {@link ServiceLoaderUtil}.
 */
public class LensControllerDelegate {
    /**
     * @see {@link LensController#isSdkAvailable()}
     */
    public boolean isSdkAvailable() {
        return false;
    }

    /** @see {@link LensController#isQueryEnabled()} */
    public boolean isQueryEnabled() {
        // Return true by default to support integration testing where
        // internal code is not available.
        return true;
    }

    /** @see {@link LensController#queryImage(LensQueryParams, Callback<LensQueryResult>)} */
    public void queryImage(
            LensQueryParams lensQueryParams, Callback<LensQueryResult> queryCallback) {}

    /** @see {@link LensController#startLensConnection()} */
    public void startLensConnection() {}

    /** @see {@link LensController#terminateLensConnections()} */
    public void terminateLensConnections() {}

    /** @see {@link LensController#terminateClassification()} */
    public void terminateClassification() {}

    /**
     * @see {@link LensController#getChipRenderParams(LensQueryParams, Callback<ChipRenderParams>)}
     */
    public void getChipRenderParams(
            LensQueryParams lensQueryParams, Callback<ChipRenderParams> chipRenderParamsCallback) {}

    /** @see {@link LensController#getShareWithGoogleLensIntent()} */
    public Intent getShareWithGoogleLensIntent(
            Uri imageUri,
            boolean isIncognito,
            String srcUrl,
            String titleOrAltText,
            String pageUrl,
            @Nullable String lensIntentType) {
        return null;
    }

    /** @see {@link LensController#startLens(WindowAndroid, Intent)} */
    public void startLens(WindowAndroid window, Intent intent) {}

    /** @see {@link LensController#startLens(WindowAndroid, Intent, LensIntentParams)} */
    public void startLens(WindowAndroid window, LensIntentParams lensIntentParams) {}

    /** @see {@link LensCOntroller#isLensEnabled(LensQueryParams)} */
    public boolean isLensEnabled(@NonNull LensQueryParams lensQueryParams) {
        return false;
    }

    /**
     * Retrieve the Text resource id for "Shop with Google Lens".
     * @return The resource id for "Shop with Google Lens" string.
     */
    protected @StringRes int getShopWithGoogleLensTextResourceId() {
        return R.string.contextmenu_shop_image_with_google_lens;
    }

    /**
     * Retrieve the Lens icon resource id.
     * Need to put the resource id on the base class to suppress the UnusedResources warning.
     * @return The resource id for Lens icon.
     */
    protected @DrawableRes int getLensIconResourceId() {
        return R.drawable.lens_icon;
    }

    /**
     * Retrieve the Text resource id for "Translate image with Google Lens".
     * @return The resource id for "Translate image with Google Lens" string.
     */
    protected @StringRes int getTranslateWithGoogleLensTextResourceId() {
        return R.string.contextmenu_translate_image_with_google_lens;
    }

    /** Enables lens debug mode for chrome://internals/lens. */
    public void enableDebugMode() {}

    /** Disables lens debug mode for chrome://internals/lens. */
    public void disableDebugMode() {}

    /** Gets debug data to populate chrome://internals/lens. */
    public String[][] getDebugData() {
        return new String[0][0];
    }
}
