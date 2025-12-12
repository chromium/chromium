// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.test.util.LeakCanaryChecker.LeakCanaryConfigProvider;
import org.chromium.build.annotations.ServiceImpl;

import java.util.Map;

@SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
@ServiceImpl(LeakCanaryConfigProvider.class)
public class CommonLeaks implements LeakCanaryConfigProvider {
    // These are leaks we want to suppress globally for all tests. They could be legitimate leaks
    // caused by //base (which we would hope to fix), or generic Android stuff that's unavoidable.

    // Android's jank FrameTracker will post delayed messages to the looper. These messages hold
    // references to our activities, but are scheduled for a few seconds delay. We could wait these
    // messages out, but it's probably better to just directly ignore them (so we can avoid flakes).
    private static final String sInteractionJankMonitorClass =
            "com.android.internal.jank.InteractionJankMonitor$Configuration";
    private static final String sInteractionJankMonitorField =
            "com.android.internal.jank.InteractionJankMonitor$Configuration#mContext";

    @Override
    public Map<String, String> getInstanceFieldLeaks() {
        return Map.of(sInteractionJankMonitorClass, sInteractionJankMonitorField);
    }
}
