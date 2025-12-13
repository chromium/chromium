// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/436110326): Move the implementation of this interface to
// org.chromium.base.process_launcher when it is launched.
package org.chromium.base;

import android.content.Context;
import android.content.ServiceConnection;

import org.chromium.build.annotations.NullMarked;

/**
 * An interface for a queue of binding requests. This is used to batch up binding requests to
 * improve performance.
 *
 * <p>This interface is not thread safe. It is expected that all methods are called on the same
 * thread.
 *
 * <p>The requests are sent to the system with the context from
 * ContextUtils.getApplicationContext().
 */
@NullMarked
public interface BindingRequestQueue {
    /**
     * Adds a rebind request to the queue.
     *
     * @param connection The connection to rebind.
     * @param flags The flags to use for the rebind.
     */
    void rebind(ServiceConnection connection, Context.BindServiceFlags flags);

    /**
     * Adds an unbind request to the queue.
     *
     * @param connection The connection to unbind.
     */
    void unbind(ServiceConnection connection);

    /** Flushes the queue, sending all pending requests. */
    void flush();
}
