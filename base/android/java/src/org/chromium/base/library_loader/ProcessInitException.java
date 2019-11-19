// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

/**
 * The exception that is thrown when the initialization of a process has failed.
 */
public class ProcessInitException extends RuntimeException {
    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     */
    public ProcessInitException(@LoaderErrors int errorCode) {
        super("errorCode=" + errorCode);
    }

    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     * @param throwable The wrapped throwable obj.
     */
    public ProcessInitException(@LoaderErrors int errorCode, Throwable throwable) {
        super("errorCode=" + errorCode, throwable);
    }
}
