// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.AppHooks;

/**
 * A class which manages communication with the Lens SDK.
 */
public class LensController {
    private static LensController sInstance;

    /**
     * @return The LensController to use during the lifetime of the browser process.
     */
    public static LensController getInstance() {
        if (sInstance == null) {
            sInstance = AppHooks.get().createLensController();
        }
        return sInstance;
    }

    /**
     * Whether the Lens SDK is available.
     * @return Whether the Lens SDK is available.
     */
    public boolean isSdkAvailable() {
        return false;
    }

    /**
     * Classify an image and once complete trigger a callback with a boolean on whether that image
     * supports a lens action.
     * @param imageUri The URI of the image to classify.
     * @param classifyCallback A callback to trigger once classification is complete.
     */
    public void classifyImage(Uri imageUri, Callback<Boolean> classifyCallback) {}

    /**
     * Classify an image and once complete trigger a callback with a boolean on whether that image
     * supports a lens action.
     * @param imageUri The URI of the image to classify.
     * @param classifyCallback A callback to trigger once classification is complete.
     * @param pageUrl Url of the top level page domain.
     * @param titleOrAltText Title or alt text of the selected image tag.
     *
     */
    public void classifyImage(Uri imageUri, String pageUrl, String titleOrAltText,
            Callback<Boolean> classifyCallback) {}

    /*
     * If an image classification request is pending but no longer needed, explicitly terminate
     * the request.
     */
    public void terminateClassification() {}
}