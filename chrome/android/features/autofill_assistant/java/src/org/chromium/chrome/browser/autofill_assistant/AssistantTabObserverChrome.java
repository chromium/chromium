// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

/**
 * Adapter hiding Chrome's ActivityTabTabObserver.
 */
public class AssistantTabObserverChrome implements Destroyable {
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;

    public AssistantTabObserverChrome(
            ActivityTabProvider activityTabProvider, AssistantTabObserver tabObserver) {
        mActivityTabObserver = new ActivityTabProvider.ActivityTabTabObserver(
                activityTabProvider, /* shouldTrigger = */ true) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                tabObserver.onObservingDifferentTab(
                        tab == null, tab != null ? tab.getWebContents() : null, hint);
            }

            @Override
            public void onActivityAttachmentChanged(
                    Tab tab, @Nullable WindowAndroid windowAndroid) {
                tabObserver.onActivityAttachmentChanged(
                        tab != null ? tab.getWebContents() : null, windowAndroid);
            }
        };
    }

    @Override
    public void destroy() {
        mActivityTabObserver.destroy();
    }
}
