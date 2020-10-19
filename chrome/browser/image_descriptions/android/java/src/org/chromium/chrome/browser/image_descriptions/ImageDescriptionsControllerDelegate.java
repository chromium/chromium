// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

/**
 * A delegate to {@link ImageDescriptionsController} to allow UIs to control its state.
 */
public interface ImageDescriptionsControllerDelegate {
    /**
     * Enable image descriptions. Calling this method will enable the image descriptions feature
     * for the current profile. Any currently opened tabs will receive image descriptions, as will
     * any future pages.
     */
    void enableImageDescriptions();

    /**
     * Disable image descriptions. Calling this method will disable the image descriptions feature
     * for the current profile. Any existing image descriptions will persist, but no new image
     * descriptions will be generated.
     */
    void disableImageDescriptions();

    /**
     * Set "Only on Wi-Fi" requirement. Calling this method will set the only on wifi user
     * preference for the current profile. If set to true, the image descriptions feature will not
     * be run while on mobile data but instead only on wifi.
     *
     * @param onlyOnWifi    Boolean - whether or not to require wifi for image descriptions.
     */
    void setOnlyOnWifiRequirement(boolean onlyOnWifi);

    /**
     * Get image descriptions once. Calling this method will fetch image descriptions one time for
     * the currently opened tab. It will not save any settings to the current profile and is
     * considered a one-off use of the feature. The method allows for setting the "Don't ask again"
     * option in shared prefs so users can easily fetch one-off descriptions bypassing the dialog.
     *
     * @param dontAskAgain  Boolean - whether or not to ask again before next one-off use.
     */
    void getImageDescriptionsJustOnce(boolean dontAskAgain);
}
