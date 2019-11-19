// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.ContentSettingsType;

import java.io.Serializable;

/**
 * Information about an object (such as a USB device) the user has granted permission for an origin
 * to access.
 */
public class ChosenObjectInfo implements Serializable {
    private final @ContentSettingsType int mContentSettingsType;
    private final String mOrigin;
    private final String mEmbedder;
    private final String mName;
    private final String mObject;
    private final boolean mIsManaged;

    ChosenObjectInfo(@ContentSettingsType int contentSettingsType, String origin, String embedder,
            String name, String object, boolean isManaged) {
        mContentSettingsType = contentSettingsType;
        mOrigin = origin;
        mEmbedder = embedder;
        mName = name;
        mObject = object;
        mIsManaged = isManaged;
    }

    /**
     * Returns the content settings type of the permission.
     */
    public @ContentSettingsType int getContentSettingsType() {
        return mContentSettingsType;
    }

    /**
     * Returns the origin that requested the permission.
     */
    public String getOrigin() {
        return mOrigin;
    }

    /**
     * Returns the origin that the requester was embedded in.
     */
    public String getEmbedder() {
        return mEmbedder;
    }

    /**
     * Returns the human readable name for the object to display in the UI.
     */
    public String getName() {
        return mName;
    }

    /**
     * Returns the opaque object string that represents the object.
     */
    public String getObject() {
        return mObject;
    }

    /**
     * Returns whether the object is managed by policy.
     */
    public boolean isManaged() {
        return mIsManaged;
    }

    /**
     * Revokes permission for the origin to access the object if the object is not managed.
     */
    public void revoke() {
        if (!mIsManaged) {
            WebsitePreferenceBridgeJni.get().revokeObjectPermission(
                    mContentSettingsType, mOrigin, mEmbedder, mObject);
        }
    }
}
