// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Observer for different tab events.
 */
public interface AssistantTabObserver {
    void onObservingDifferentTab(
            boolean isTabNull, @Nullable WebContents webContents, boolean isHint);

    void onActivityAttachmentChanged(
            @Nullable WebContents webContents, @Nullable WindowAndroid windowAndroid);
}
