// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.MessageQueue;
import android.os.MessageQueue.IdleHandler;

import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLegacyMessageQueue;

import java.util.ArrayList;
import java.util.List;

/** Shadow MessageQueue implementation that adds support for IdleHandler(s). */
@Implements(MessageQueue.class)
public class ShadowIdleHandlerAwareMessageQueue extends ShadowLegacyMessageQueue {
    private final List<IdleHandler> mIdleHandlers = new ArrayList<>();
    private final Object mIdleHandlersLock = new Object();

    /** Default constructor needed by robolectric. */
    public ShadowIdleHandlerAwareMessageQueue() {}

    /** @see MessageQueue#addIdleHandler(IdleHandler) */
    public void addIdleHandler(IdleHandler handler) {
        synchronized (mIdleHandlersLock) {
            mIdleHandlers.add(handler);
        }
    }

    /** @see MessageQueue#removeIdleHandler(IdleHandler) */
    public void removeIdleHandler(IdleHandler handler) {
        synchronized (mIdleHandlersLock) {
            mIdleHandlers.remove(handler);
        }
    }

    /** Run all idle handlers. */
    public void runIdleHandlers() {
        List<IdleHandler> idleHandlers;
        synchronized (mIdleHandlersLock) {
            idleHandlers = new ArrayList<>(mIdleHandlers);
        }
        for (int i = 0; i < idleHandlers.size(); i++) {
            IdleHandler handler = idleHandlers.get(i);
            if (!handler.queueIdle()) removeIdleHandler(handler);
        }
    }

    public List<IdleHandler> getIdleHandlers() {
        return mIdleHandlers;
    }

    public void clearIdleHandlers() {
        synchronized (mIdleHandlersLock) {
            mIdleHandlers.clear();
        }
    }
}
