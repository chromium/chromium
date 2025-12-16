// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.memory_leaks;

import org.chromium.base.test.util.LeakCanaryChecker.LeakCanaryConfigProvider;
import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.ServiceImpl;

import java.util.List;
import java.util.Map;

@SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
@ServiceImpl(LeakCanaryConfigProvider.class)
public class ChromeInstrumentationLeaks implements LeakCanaryConfigProvider {
    // This class is a collection of known leaks in Chrome Instrumentation tests (eg.
    // chrome_public_test_apk). The goal is to  burn this class down to nothing by fixing leaks.
    // Please include a bug for each leak.

    // crbug.com/467345468
    @IdentifierNameString
    private static String sClass467345468 = "org.chromium.chrome.browser.metrics.UmaSessionStats$1";

    @IdentifierNameString
    private static String sField467345468 =
            "org.chromium.chrome.browser.metrics.UmaSessionStats$1#this$0";

    @Override
    public Map<String, String> getInstanceFieldLeaks() {
        return Map.of(sClass467345468, sField467345468);
    }

    // crbug.com/462704925
    @IdentifierNameString
    private static String sClass462704925 = "org.chromium.ui.KeyboardVisibilityDelegate";

    @IdentifierNameString
    private static String sField462704925 = "org.chromium.ui.KeyboardVisibilityDelegate#sInstance";

    // crbug.com/462709210
    @IdentifierNameString
    private static String sClass462709210 =
            "org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager$LazyHolder";

    @IdentifierNameString
    private static String sField462709210 =
            "org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager$LazyHolder#sInstance";

    @Override
    public Map<String, String> getStaticFieldLeaks() {
        return Map.of(sClass462709210, sField462709210, sClass462704925, sField462704925);
    }

    @Override
    public List<String> getJavaLocalLeaks() {
        // crbug.com/465145691
        return List.of("AsyncLayoutInflator");
    }
}
