// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.base.annotations.CalledByNative;

/**
 * SendTabToSelfEntry mirrors the native struct send_tab_to_self::SendTabToSelfEntry declared in
 * //components/send_tab_to_self/send_tab_to_self_entry.h. This provides useful getters and methods
 * called by native code.
 */
public class SendTabToSelfEntry {
    /** The unique random id for the entry. */
    public final String guid;
    /** The mUrl of the page the user would like to send to themselves. */
    public final String url;
    /** The mTitle of the entry. Might be empty. */
    public final String title;
    /** The name of the device that originated the sent tab. */
    public final String deviceName;
    /** The time that the tab was shared. */
    public final long sharedTime;
    /** The time that the tab was navigated to. */
    public final long originalNavigationTime;
    /** The cache guid of the target device. */
    public final String targetDeviceSyncCacheGuid;

    public SendTabToSelfEntry(String guid, String url, String title, long sharedTime,
            long originalNavigationTime, String deviceName, String targetDeviceSyncCacheGuid) {
        this.guid = guid;
        this.url = url;
        this.title = title;
        this.sharedTime = sharedTime;
        this.originalNavigationTime = originalNavigationTime;
        this.deviceName = deviceName;
        this.targetDeviceSyncCacheGuid = targetDeviceSyncCacheGuid;
    }

    /** Used by native code in order to create a new object of this type. */
    @CalledByNative
    private static SendTabToSelfEntry createSendTabToSelfEntry(String mGuid, String mUrl,
            String mTitle, long mSharedTime, long mOriginalNavigationTime, String mDeviceName,
            String targetDeviceSyncCacheGuid) {
        return new SendTabToSelfEntry(mGuid, mUrl, mTitle, mSharedTime, mOriginalNavigationTime,
                mDeviceName, targetDeviceSyncCacheGuid);
    }
}
