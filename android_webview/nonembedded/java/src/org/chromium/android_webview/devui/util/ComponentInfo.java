// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import androidx.annotation.NonNull;

import java.util.Objects;

/** A model class with info about the components. */
public class ComponentInfo {
    @NonNull private final String mComponentName;
    @NonNull private final String mComponentVersion;

    public ComponentInfo(@NonNull String name, @NonNull String version) {
        mComponentName = name;
        mComponentVersion = version;
    }

    public String getComponentName() {
        return mComponentName;
    }

    public String getComponentVersion() {
        return mComponentVersion;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || (getClass() != obj.getClass())) {
            return false;
        }

        ComponentInfo item2 = (ComponentInfo) obj;
        return mComponentName.equals(item2.mComponentName)
                && mComponentVersion.equals(item2.mComponentVersion);
    }

    @Override
    public String toString() {
        return "Name : " + mComponentName + " - " + "Version : " + mComponentVersion;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mComponentName, mComponentVersion);
    }
}
