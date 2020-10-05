// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.chromium.chrome.browser.base.SplitCompatJobService;

/** See {@link NotificationJobServiceImpl}. */
public class NotificationJobService extends SplitCompatJobService {
    public NotificationJobService() {
        super("org.chromium.chrome.browser.notifications.NotificationJobServiceImpl");
    }
}
