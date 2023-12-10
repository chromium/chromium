// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** Exposes services from {@link ChromeMediaNotificationControllerDelegate} in the base module. */
public class ChromeMediaNotificationControllerServices {
    /** See {@link ChromeMediaNotificationControllerDelegate$PlaybackListenerServiceImpl}. */
    public static class PlaybackListenerService extends SplitCompatService {
        private static @IdentifierNameString String sImplClassName =
                "org.chromium.chrome.browser.media.ui."
                        + "ChromeMediaNotificationControllerDelegate$PlaybackListenerServiceImpl";

        public PlaybackListenerService() {
            super(sImplClassName);
        }
    }

    /** See {@link ChromeMediaNotificationControllerDelegate$PresentationListenerServiceImpl}. */
    public static class PresentationListenerService extends SplitCompatService {
        private static @IdentifierNameString String sImplClassName =
                "org.chromium.chrome.browser.media.ui."
                        + "ChromeMediaNotificationControllerDelegate$PresentationListenerServiceImpl";

        public PresentationListenerService() {
            super(sImplClassName);
        }
    }

    /** See {@link ChromeMediaNotificationControllerDelegate$CastListenerServiceImpl}. */
    public static class CastListenerService extends SplitCompatService {
        private static @IdentifierNameString String sImplClassName =
                "org.chromium.chrome.browser.media.ui."
                        + "ChromeMediaNotificationControllerDelegate$CastListenerServiceImpl";

        public CastListenerService() {
            super(sImplClassName);
        }
    }
}
