// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;

import org.chromium.base.test.util.JniMocker;

public class BookmarkNativesMockRule implements TestRule {
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() {
                mJniMocker.mock(
                        BookmarkBridgeJni.TEST_HOOKS, Mockito.mock(BookmarkBridge.Natives.class));
            }
        };
    }
}
