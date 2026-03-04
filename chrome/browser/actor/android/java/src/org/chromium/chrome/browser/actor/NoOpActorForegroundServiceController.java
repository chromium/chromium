// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;

import org.chromium.build.annotations.NullMarked;

/** No-op implementation of {@link ActorForegroundServiceController}. */
@NullMarked
public class NoOpActorForegroundServiceController implements ActorForegroundServiceController {
    private static class Holder {
        private static final NoOpActorForegroundServiceController INSTANCE =
                new NoOpActorForegroundServiceController();
    }

    public static NoOpActorForegroundServiceController getInstance() {
        return Holder.INSTANCE;
    }

    private NoOpActorForegroundServiceController() {}

    @Override
    public void startAndBindService(Runnable onConnected) {}

    @Override
    public void unbindService() {}

    @Override
    public boolean isConnected() {
        return false;
    }

    @Override
    public void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification) {}

    @Override
    public void stopActorForegroundService(int flags) {}
}
