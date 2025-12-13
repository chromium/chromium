// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.annotation.SuppressLint;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.ui.modelutil.LayoutViewBuilder;

/** Manages the creation of {@link MessageHostDelegate}s. */
@NullMarked
public class MessageHostDelegateFactory {
    /**
     * Builds a {@link MessageHostDelegate} for the given {@link TabListCoordinator}.
     *
     * @param tabListCoordinator The {@link TabListCoordinator} to build the operations for.
     * @return The built {@link MessageHostDelegate}.
     */
    public static MessageHostDelegate<@MessageType Integer, @UiType Integer> build(
            TabListCoordinator tabListCoordinator) {
        return new MessageHostDelegate<@MessageType Integer, @UiType Integer>() {
            @SuppressLint("WrongConstant")
            @Override
            public void registerService(
                    MessageService<@MessageType Integer, @UiType Integer> service) {
                tabListCoordinator.registerItemType(
                        service.getUiType(),
                        new LayoutViewBuilder<>(service.getLayout()),
                        service.getBinder());
            }
        };
    }
}
