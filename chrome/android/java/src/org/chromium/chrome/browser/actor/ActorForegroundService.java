// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.SplitCompatService;
import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;

/**
 * Foreground service for managing actor tasks. This class is a wrapper that loads {@link
 * ActorForegroundServiceImpl}.
 */
@NullMarked
public class ActorForegroundService extends SplitCompatService {
    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.actor.ActorForegroundServiceImpl";

    public ActorForegroundService() {
        super(sImplClassName);
    }
}
