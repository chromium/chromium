// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

/**
 * Tasks posted will be run in order with respect to this sequence, but they may be executed
 * on arbitrary threads. Unless specified otherwise by the provider of a given
 * SequencedTaskRunner, tasks posted to it have no ordering, nor mutual exclusion, execution
 * guarantees w.r.t. other SequencedTaskRunners. They have destroy() automatically called whenever
 * they go empty, so calling destroy() on them is not needed.
 */
public interface SequencedTaskRunner extends TaskRunner {}
