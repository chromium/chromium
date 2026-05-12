// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.LeakCanaryChecker.DisableLeakChecks;
import org.chromium.base.test.util.LeakCanaryChecker.EnableLeakChecks;

/** Tests for {@link LeakCanaryChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LeakCanaryCheckerTest {

    @Test
    public void testIsLikelyTestLeak_noTrace() {
        String message = "Some random error message";
        Assert.assertFalse(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_traceWithKeyword() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.chromium.chrome.browser.MockTab instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertTrue(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_traceWithoutKeyword() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.chromium.chrome.browser.Tab instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertFalse(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_keywordInReference_notNode() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.chromium.chrome.browser.Tab instance\n"
                        + "│    ↓ Tab.mTestObserver\n"
                        + "├─ org.chromium.chrome.browser.Observer instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertFalse(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_wholeWordOnly() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.chromium.chrome.browser.NotatestTab instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertFalse(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_traceWithMockingKeyword() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.mockito.internal.progress.MockingProgressImpl instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertTrue(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @Test
    public void testIsLikelyTestLeak_traceWithStubbingKeyword() {
        String message =
                "┬───\n"
                        + "│ GC Root: Local variable in native code\n"
                        + "│\n"
                        + "├─ org.mockito.internal.stubbing.OngoingStubbingImpl instance\n"
                        + "│    Leaking: YES\n"
                        + "╰→ ...";
        Assert.assertTrue(LeakCanaryChecker.isLikelyTestLeak(message));
    }

    @EnableLeakChecks
    @DisableLeakChecks("crbug.com/123456")
    public static class BothAnnotationsTest {}

    @Test
    public void testIsEnabled_bothAnnotations_throws() {
        Assert.assertThrows(
                IllegalStateException.class,
                () -> LeakCanaryChecker.isEnabled(BothAnnotationsTest.class));
    }
}
