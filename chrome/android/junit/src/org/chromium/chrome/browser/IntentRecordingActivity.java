// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Test {@link Activity} that records Intents started from this Activity. Unlike Robolectric's
 * started-activity tracking, which is shared by every Context, this tells Activity-initiated starts
 * apart from ApplicationContext starts.
 */
@NullMarked
public class IntentRecordingActivity extends Activity {
    private final List<Intent> mStartedIntents = new ArrayList<>();

    // All Activity#startActivity() overloads route through this method.
    @Override
    public void startActivityForResult(Intent intent, int requestCode, @Nullable Bundle options) {
        mStartedIntents.add(intent);
        super.startActivityForResult(intent, requestCode, options);
    }

    /** Returns the Intents started from this Activity, in order. */
    public List<Intent> getStartedIntents() {
        return mStartedIntents;
    }
}
