// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/** This class is a shim that wraps the JNI interface to the C++-side checks. */
public class PageInfoSharingBridge {
    /**
     * Determines if the current profile supports page info sharing.
     *
     * @param profile The profile to check.
     * @return True if the current profile supports page info sharing.
     */
    public static boolean doesProfileSupportPageInfo(Profile profile) {
        assert profile != null;
        return PageInfoSharingBridgeJni.get().doesProfileSupportPageInfo(profile);
    }

    /**
     * Determines if the current tab can be shared at this moment.
     *
     * @param tab A tab to share;
     * @return True if the current tab supports page info sharing.
     */
    public static boolean doesTabSupportPageInfo(Tab tab) {
        assert tab != null;
        return PageInfoSharingBridgeJni.get().doesTabSupportPageInfo(tab);
    }

    @NativeMethods
    public interface Natives {
        boolean doesProfileSupportPageInfo(@JniType("Profile*") Profile profile);

        boolean doesTabSupportPageInfo(Tab tab);
    }
}
