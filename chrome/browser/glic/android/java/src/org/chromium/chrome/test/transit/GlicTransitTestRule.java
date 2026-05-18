// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.junit.rules.RuleChain;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicEnabling;

/** Rule for integration tests that reuse ChromeTabbedActivity with Glic enabled for testing. */
@NullMarked
public class GlicTransitTestRule extends AutoResetCtaTransitTestRule {
    private final RuleChain mGlicChain;

    public GlicTransitTestRule() {
        super(/* clearAllTabState= */ true);
        mGlicChain =
                RuleChain.outerRule(
                                (base, desc) ->
                                        new Statement() {
                                            @Override
                                            public void evaluate() throws Throwable {
                                                GlicEnabling.setEnabledForTesting(
                                                        true, /* forwardToNative= */ true);
                                                base.evaluate();
                                            }
                                        })
                        .around(super::apply);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mGlicChain.apply(statement, description);
    }
}
