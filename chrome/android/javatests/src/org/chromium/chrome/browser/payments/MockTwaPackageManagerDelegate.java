// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;

import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.Map;

/** Simulates a TWA package manager in memory. */
class MockTwaPackageManagerDelegate extends TwaPackageManagerDelegate {
    private String mMockTwaPackage;
    // A map of a package name to its installer's package name.
    private Map<String, String> mMockInstallerPackageMap = new HashMap<>();

    /**
     * Mock the current package to be a Trust Web Activity package.
     * @param mockTwaPackage The intended package nam, not allowed to be null.
     */
    public void setMockTrustedWebActivity(String mockTwaPackage) {
        assert mockTwaPackage != null;
        mMockTwaPackage = mockTwaPackage;
    }

    /**
     * Mock the installer of a specified package.
     * @param packageName The package name that is intended to mock a installer for, not allowed to
     *         be null.
     * @param installerPackageName The package name intended to be set as the installer of the
     *         specified package.
     */
    public void mockInstallerForPackage(String packageName, @Nullable String installerPackageName) {
        assert packageName != null;
        mMockInstallerPackageMap.put(packageName, installerPackageName);
    }

    @Override
    @Nullable
    public String getTwaPackageName(Activity activity) {
        return mMockTwaPackage;
    }

    @Override
    @Nullable
    public String getInstallerPackage(String packageName) {
        return mMockInstallerPackageMap.get(packageName);
    }
}
