// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.build.annotations.NullMarked;

/** The exception that is thrown when the initialization of a process has failed. */
@NullMarked
public class ProcessInitException extends RuntimeException {
    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     */
    public ProcessInitException(@LoaderErrors int errorCode) {
        super("errorCode=" + errorCode);
    }

    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     * @param failureCause The wrapped Throwable obj.
     */
    public ProcessInitException(@LoaderErrors int errorCode, Throwable failureCause) {
        super("errorCode=" + errorCode, failureCause);
    }
}
