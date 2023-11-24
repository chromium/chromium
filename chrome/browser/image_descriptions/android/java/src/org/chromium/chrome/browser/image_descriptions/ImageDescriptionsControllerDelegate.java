// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** A delegate to {@link ImageDescriptionsController} to allow UIs to control its state. */
public interface ImageDescriptionsControllerDelegate {
    /**
     * Enable image descriptions for the given Profile. Any currently opened tabs for this profile
     * will receive image descriptions, as will any future pages.
     * @param profile       Profile - the profile to enable descriptions for.
     */
    void enableImageDescriptions(Profile profile);

    /**
     * Disable image descriptions for the given Profile. No new image descriptions will be generated
     * for any tabs of the given profile.
     * @param profile       Profile - the profile to disable descriptions for.
     */
    void disableImageDescriptions(Profile profile);

    /**
     * Set "Only on Wi-Fi" requirement for the given profile.
     * If set to true, the image descriptions feature will not be run while on mobile data.
     *
     * @param onlyOnWifi    Boolean - whether or not to require wifi for image descriptions.
     * @param profile   Profile - the profile to set requirement on.
     */
    void setOnlyOnWifiRequirement(boolean onlyOnWifi, Profile profile);

    /**
     * Get image descriptions once. Calling this method will fetch image descriptions one time for
     * the currently opened tab. It will not save any settings to the current profile and is
     * considered a one-off use of the feature. The method allows for setting the "Don't ask again"
     * option in shared prefs so users can easily fetch one-off descriptions bypassing the dialog.
     *
     * @param dontAskAgain  Boolean - whether or not to ask again before next one-off use.
     * @param webContents   WebContents - The web contents of the tab to get descriptions for.
     */
    void getImageDescriptionsJustOnce(boolean dontAskAgain, WebContents webContents);
}
