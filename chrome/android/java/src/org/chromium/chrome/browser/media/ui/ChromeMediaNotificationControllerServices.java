// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** Exposes services from {@link ChromeMediaNotificationControllerDelegate} in the base module. */
public class ChromeMediaNotificationControllerServices {
    /** See {@link ChromeMediaNotificationControllerDelegate$PlaybackListenerServiceImpl}. */
    public static class PlaybackListenerService extends SplitCompatService {
        public PlaybackListenerService() {
            super(SplitCompatUtils.getIdentifierName("org.chromium.chrome.browser.media.ui."
                    + "ChromeMediaNotificationControllerDelegate$PlaybackListenerServiceImpl"));
        }
    }

    /** See {@link ChromeMediaNotificationControllerDelegate$PresentationListenerServiceImpl}. */
    public static class PresentationListenerService extends SplitCompatService {
        public PresentationListenerService() {
            super(SplitCompatUtils.getIdentifierName("org.chromium.chrome.browser.media.ui."
                    + "ChromeMediaNotificationControllerDelegate$PresentationListenerServiceImpl"));
        }
    }

    /** See {@link ChromeMediaNotificationControllerDelegate$CastListenerServiceImpl}. */
    public static class CastListenerService extends SplitCompatService {
        public CastListenerService() {
            super(SplitCompatUtils.getIdentifierName("org.chromium.chrome.browser.media.ui."
                    + "ChromeMediaNotificationControllerDelegate$CastListenerServiceImpl"));
        }
    }
}
