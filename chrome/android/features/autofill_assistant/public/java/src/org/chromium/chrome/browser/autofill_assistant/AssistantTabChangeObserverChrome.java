// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.autofill_assistant.AssistantTabChangeObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Adapter hiding Chrome's ActivityTabTabObserver.
 */
public class AssistantTabChangeObserverChrome implements Destroyable {
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;

    public AssistantTabChangeObserverChrome(
            ActivityTabProvider activityTabProvider, AssistantTabChangeObserver tabChangeObserver) {
        mActivityTabObserver = new ActivityTabProvider.ActivityTabTabObserver(
                activityTabProvider, /* shouldTrigger = */ true) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                tabChangeObserver.onObservingDifferentTab(
                        tab == null, tab != null ? tab.getWebContents() : null, hint);
            }

            @Override
            public void onActivityAttachmentChanged(
                    Tab tab, @Nullable WindowAndroid windowAndroid) {
                tabChangeObserver.onActivityAttachmentChanged(
                        tab != null ? tab.getWebContents() : null, windowAndroid);
            }

            @Override
            public void onContentChanged(Tab tab) {
                tabChangeObserver.onContentChanged(tab.getWebContents());
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                tabChangeObserver.onWebContentsSwapped(
                        tab.getWebContents(), didStartLoad, didFinishLoad);
            }

            @Override
            public void onDestroyed(Tab tab) {
                tabChangeObserver.onDestroyed(tab.getWebContents());
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                tabChangeObserver.onInteractabilityChanged(tab.getWebContents(), isInteractable);
            }
        };
    }

    @Override
    public void destroy() {
        mActivityTabObserver.destroy();
    }
}
