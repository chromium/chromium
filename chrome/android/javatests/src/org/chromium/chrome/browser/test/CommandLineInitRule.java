// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;

import java.util.Arrays;

/**
 * JUnit rule for setting command line arguments before initializing the browser.
 *
 * <p>Having it as its own rule allows us to chain it using RuleChain and allows it to be explicitly
 * run prior to initializing the browser.
 */
public class CommandLineInitRule implements TestRule {
    private String[] mArgs;

    public CommandLineInitRule(String[] args) {
        if (args != null) {
            mArgs = Arrays.copyOf(args, args.length);
        }
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                CommandLine.init(mArgs);
                base.evaluate();
            }
        };
    }
}
