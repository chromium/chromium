// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

/** Constants to be used by child processes. */
public interface ChildProcessConstants {
    // Below are the names for the items placed in the bind or start command intent.
    // Note that because that intent maybe reused if a service is restarted, none should be process
    // specific.

    public static final String EXTRA_BIND_TO_CALLER =
            "org.chromium.base.process_launcher.extra.bind_to_caller";

    // Below are the names for the items placed in the Bundle passed in the
    // IChildProcessService.setupConnection call, once the connection has been established.

    // Key for the command line.
    public static final String EXTRA_COMMAND_LINE =
            "org.chromium.base.process_launcher.extra.command_line";

    // Key for the file descriptors that should be mapped in the child process.
    public static final String EXTRA_FILES = "org.chromium.base.process_launcher.extra.extraFiles";

    // Key for the browser package name.
    public static final String EXTRA_BROWSER_PACKAGE_NAME =
            "org.chromium.base.process_launcher.extra.browser_package_name";
}
