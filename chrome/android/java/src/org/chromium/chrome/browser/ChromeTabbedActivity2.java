// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

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
}
