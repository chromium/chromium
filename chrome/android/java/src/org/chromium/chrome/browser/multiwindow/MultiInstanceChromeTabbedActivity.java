// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.chrome.browser.ChromeTabbedActivity;

/**
 * A subclass of ChromeTabbedActivity, used in Samsung multi-instance mode (before Android N).
 *
 * Unlike ChromeTabbedActivity, this activity does not have launchMode="singleTask" in the manifest,
 * so multiple instances of this activity can be launched. Also, this activity can live in the same
 * task as ChromeLauncherActivity, which is needed to support Samsung multi-instance.
 */
public class MultiInstanceChromeTabbedActivity extends ChromeTabbedActivity {}