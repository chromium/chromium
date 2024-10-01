// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.lens;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages communication with the Lens SDK. */
public class LensController {
    private static LensController sInstance = new LensController();

    private final LensControllerDelegate mDelegate;

    /**
     * @return The singleton instance of LensController.
     */
    public static LensController getInstance() {
        return sInstance;
    }

    public LensController() {
        LensControllerDelegate delegate =
                ServiceLoaderUtil.maybeCreate(LensControllerDelegate.class);
        if (delegate == null) {
            delegate = new LensControllerDelegate();
        }
        mDelegate = delegate;
    }

    /**
     * Whether the Lens SDK is available.
     *
     * @return Whether the Lens SDK is available.
     */
    public boolean isSdkAvailable() {
        return false;
    }

    /**
     * Whether the Lens chip should be enabled based on user signals.
     * @return Whether the Lens SDK is available.
     */
    public boolean isQueryEnabled() {
        return mDelegate.isQueryEnabled();
    }

    /**
     * Classify an image and once complete trigger a callback with a LensQueryResult on whether that
     * image supports a lens action.
     * @param LensQueryParams A wrapper object which contains params for the Lens image query.
     * @param queryCallback A callback to trigger once classification is complete.
     *
     */
    public void queryImage(
            LensQueryParams lensQueryParams, Callback<LensQueryResult> queryCallback) {
        mDelegate.queryImage(lensQueryParams, queryCallback);
    }

    /*
     * If an image classification request is pending but no longer needed, explicitly terminate
     * the request.
     */
    public void terminateClassification() {
        mDelegate.terminateClassification();
    }

    /**
     * Get the data to generate a chip as an entry point to Lens.
     * Classify an image and return chip data once the classification completes.
     * @param lensQueryParams A wrapper object which contains params for the image classification
     *         query.
     * @param chipRenderParamsCallback A callback to trigger once the classification is compelete.
     */
    public void getChipRenderParams(
            LensQueryParams lensQueryParams, Callback<ChipRenderParams> chipRenderParamsCallback) {
        mDelegate.getChipRenderParams(lensQueryParams, chipRenderParamsCallback);
    }

    /**
     * Launch lens with an intent.
     * @param window The current window.
     * @param lensIntentParams The intent parameters for Lens
     */
    public void startLens(WindowAndroid window, LensIntentParams lensIntentParams) {
        mDelegate.startLens(window, lensIntentParams);
    }

    /** Starts the Lens connection. */
    public void startLensConnection() {
        mDelegate.startLensConnection();
    }

    /** Terminate any active Lens connections. */
    public void terminateLensConnections() {
        mDelegate.terminateLensConnections();
    }

    // TODO(b/180960783): Revisit the wrapper object for this enablement check. LensQueryParams
    // was designed to be only used in the Prime classification query.
    /**
     * Whether the Lens is enabled based on user signals.
     * @param lensQueryParams A wrapper object which contains params for the enablement check.
     * @return True if Lens is enabled.
     */
    public boolean isLensEnabled(@NonNull LensQueryParams lensQueryParams) {
        return mDelegate.isLensEnabled(lensQueryParams);
    }

    /** Enables lens debug mode for chrome://internals/lens. */
    public void enableDebugMode() {
        mDelegate.enableDebugMode();
    }

    /** Disables lens debug mode for chrome://internals/lens. */
    public void disableDebugMode() {
        mDelegate.disableDebugMode();
    }

    /** Gets debug data to populate chrome://internals/lens. */
    public String[][] getDebugData() {
        return mDelegate.getDebugData();
    }
}
