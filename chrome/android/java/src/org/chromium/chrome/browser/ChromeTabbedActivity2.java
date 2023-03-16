// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

/**
 * A subclass of ChromeTabbedActivity, used in Android N multi-window mode.
 *
 * This activity can appear side-by-side with ChromeTabbedActivity in multi-window mode. It has a
 * separate set of tabs, as determined by logic in TabWindowManager.
 *
 * Since ChromeTabbedActivity has launchMode="singleTask" in the manifest, there can't be two
 * instances of ChromeTabbedActivity; hence this activity is needed. Moreover, having separately-
 * named activities makes it possible to bring either existing activity to the foreground on the
 * desired side of the screen when firing an intent.
 */
public class ChromeTabbedActivity2 extends ChromeTabbedActivity {
    @Override
    protected boolean isFirstActivity() {
        return false;
    }

    @Override
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(
            Intent intent, Bundle savedInstanceState) {
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // ChromeTabbedActivity2 can be launched in multi-instance configuration if a CTA2-task
            // survives Chrome upgrade and gets to the foreground to have the activity re-created.
            // Bounce to ChromeTabbedActivity and kill the CTA2-task.
            int windowId = MultiWindowUtils.INVALID_INSTANCE_ID;
            if (savedInstanceState != null) {
                windowId = savedInstanceState.getInt(WINDOW_INDEX, windowId);
            }
            Intent newIntent =
                    MultiWindowUtils.createNewWindowIntent(this, windowId, false, false, true);
            startActivity(newIntent, savedInstanceState);
            return LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK;
        }
        return super.maybeDispatchLaunchIntent(intent, savedInstanceState);
    }
}
