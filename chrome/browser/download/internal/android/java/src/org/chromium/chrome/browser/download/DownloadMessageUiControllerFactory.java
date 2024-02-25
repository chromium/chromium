// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.chrome.browser.download.DownloadMessageUiController.Delegate;

/** Factory class to build a DownloadMessageUiController instance. */
public class DownloadMessageUiControllerFactory {
    private DownloadMessageUiControllerFactory() {}

    /** Builds a {@link DownloadMessageUiControllerImpl} instance. */
    public static DownloadMessageUiController create(Delegate delegate) {
        return new DownloadMessageUiControllerImpl(delegate);
    }
}
