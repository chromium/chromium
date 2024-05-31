// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.profiles.Profile;

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
}
