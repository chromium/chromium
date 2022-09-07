// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

/**
 * Tasks posted will be run in order on a single thread. Multiple SingleThreadTaskRunners
 * can share a single thread. When sharing a thread, mutual exclusion is guaranteed but
 * unless specified otherwise by the provider of a given SingleThreadTaskRunner there are
 * no ordering guarantees w.r.t. other SingleThreadTaskRunner.
 */
public interface SingleThreadTaskRunner extends SequencedTaskRunner {
    /**
     *
     * @return true iff this SingleThreadTaskRunner is bound to the current thread.
     */
    boolean belongsToCurrentThread();
}
