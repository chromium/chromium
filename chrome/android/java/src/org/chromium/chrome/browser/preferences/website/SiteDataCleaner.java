// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Encapsulates clearing the data of {@link Website}s.
 * Requires native library to be initialized.
 */
public class SiteDataCleaner {
    /**
     * Clears the data of the specified site.
     * @param finishCallback is called when finished.
     */
    public void clearData(Website site, Runnable finishCallback) {
        String origin = site.getAddress().getOrigin();
        WebsitePreferenceBridgeJni.get().clearCookieData(origin);
        WebsitePreferenceBridgeJni.get().clearBannerData(origin);
        WebsitePreferenceBridgeJni.get().clearMediaLicenses(origin);

        // Clear the permissions.
        for (@ContentSettingException.Type int type = 0;
                type < ContentSettingException.Type.NUM_ENTRIES; type++) {
            site.setContentSettingPermission(type, ContentSettingValues.DEFAULT);
        }
        for (@PermissionInfo.Type int type = 0; type < PermissionInfo.Type.NUM_ENTRIES; type++) {
            site.setPermission(type, ContentSettingValues.DEFAULT);
        }

        for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
            info.revoke();
        }

        site.clearAllStoredData(finishCallback::run);
    }
}
