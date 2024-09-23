// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.HashMap;

/** Java implementation of UnusedSitePermissionsBridge for testing. */
class FakeUnusedSitePermissionsBridge implements UnusedSitePermissionsBridge.Natives {
    private HashMap<String, PermissionsData> mUnusedPermissionsDataMap = new HashMap<>();

    public void setPermissionsDataForReview(PermissionsData[] permissionsDataList) {
        for (PermissionsData permissionsData : permissionsDataList) {
            mUnusedPermissionsDataMap.put(permissionsData.getOrigin(), permissionsData);
        }
    }

    @Override
    public PermissionsData[] getRevokedPermissions(Profile profile) {
        return mUnusedPermissionsDataMap.values().toArray(new PermissionsData[0]);
    }

    @Override
    public void regrantPermissions(Profile profile, String primaryPattern) {
        mUnusedPermissionsDataMap.remove(primaryPattern);
    }

    @Override
    public void undoRegrantPermissions(Profile profile, PermissionsData permissionsData) {
        mUnusedPermissionsDataMap.put(permissionsData.getOrigin(), permissionsData);
    }

    @Override
    public void clearRevokedPermissionsReviewList(Profile profile) {
        mUnusedPermissionsDataMap.clear();
    }

    @Override
    public void restoreRevokedPermissionsReviewList(
            Profile profile, PermissionsData[] permissionsDataList) {
        setPermissionsDataForReview(permissionsDataList);
    }

    @Override
    public String[] contentSettingsTypeToString(int[] contentSettingsTypeList) {
        String[] contentSettingsNamesList = new String[contentSettingsTypeList.length];
        for (int i = 0; i < contentSettingsTypeList.length; i++) {
            String contentSettingsName = "default";
            switch (contentSettingsTypeList[i]) {
                case ContentSettingsType.MEDIASTREAM_CAMERA:
                    contentSettingsName = "Camera";
                    break;
                case ContentSettingsType.MEDIASTREAM_MIC:
                    contentSettingsName = "Microphone";
                    break;
                case ContentSettingsType.GEOLOCATION:
                    contentSettingsName = "Location";
                    break;
                case ContentSettingsType.BACKGROUND_SYNC:
                    contentSettingsName = "Background sync";
                    break;
                default:
                    assert false : "Unreached";
            }
            contentSettingsNamesList[i] = contentSettingsName;
        }
        return contentSettingsNamesList;
    }
}
