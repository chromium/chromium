// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.build.annotations.NullMarked;

/** Constants to be used by child processes. */
@NullMarked
public interface ChildProcessConstants {
    // Below are the names for the items placed in the bind or start command intent.
    // Note that because that intent maybe reused if a service is restarted, none should be process
    // specific.

    public static final String EXTRA_BIND_TO_CALLER =
            "org.chromium.base.process_launcher.extra.bind_to_caller";

    // Key for the browser package name.
    public static final String EXTRA_BROWSER_PACKAGE_NAME =
            "org.chromium.base.process_launcher.extra.browser_package_name";
}
