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

    // crbug.com/462704925
    @IdentifierNameString
    private static String sClass462704925 = "org.chromium.ui.KeyboardVisibilityDelegate";

    @IdentifierNameString
    private static String sField462704925 = "org.chromium.ui.KeyboardVisibilityDelegate#sInstance";

    // In the rare case that the cleanup task hasn't finished yet, ignore the "leak" - it gets
    // cleaned up once the cleanup task happens.
    @IdentifierNameString
    private static String sClassPersistencePolicyCleanup =
            "org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy";

    @IdentifierNameString
    private static String sFieldPersistencePolicyCleanup =
            "org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy#sCleanupTask";

    @Override
    public Map<String, String> getStaticFieldLeaks() {
        return Map.of(
                sClass462704925,
                sField462704925,
                sClassPersistencePolicyCleanup,
                sFieldPersistencePolicyCleanup);
    }

    // crbug.com/478251814
    @IdentifierNameString
    private static String sClass478251814 = "org.chromium.chrome.browser.logo.LogoMediator";

    @IdentifierNameString
    private static String sField478251814 =
            "org.chromium.chrome.browser.logo.LogoMediator#mContext";

    @Override
    public Map<String, String> getInstanceFieldLeaks() {
        return Map.of(sClass478251814, sField478251814);
    }

    @Override
    public List<String> getJavaLocalLeaks() {
        // crbug.com/465145691
        return List.of("AsyncLayoutInflator");
    }
}
