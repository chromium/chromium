// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.view.Choreographer;

/**
 * An adapter that allows PostTask to submit Choreographer frame callbacks which
 * run after the next vsync.
 */
final class ChoreographerTaskRunner implements SingleThreadTaskRunner {
    private final Choreographer mChoreographer;

    ChoreographerTaskRunner(Choreographer choreographer) {
        mChoreographer = choreographer;
    }

    @Override
    public boolean belongsToCurrentThread() {
        try {
            return mChoreographer == Choreographer.getInstance();
        } catch (IllegalStateException e) {
            return false;
        }
    }

    @Override
    public void postTask(Runnable task) {
        mChoreographer.postFrameCallback(new Choreographer.FrameCallback() {
            @Override
            public void doFrame(long frameTimeNanos) {
                task.run();
            }
        });
    }

    @Override
    public void destroy() {
        // NOP
    }

    @Override
    public void disableLifetimeCheck() {
        // NOP
    }

    @Override
    public void postDelayedTask(Runnable task, long delayMillis) {
        mChoreographer.postFrameCallbackDelayed(new Choreographer.FrameCallback() {
            @Override
            public void doFrame(long frameTimeNanos) {
                task.run();
            }
        }, delayMillis);
    }

    @Override
    public void initNativeTaskRunner() {
        // NOP
    }
}
