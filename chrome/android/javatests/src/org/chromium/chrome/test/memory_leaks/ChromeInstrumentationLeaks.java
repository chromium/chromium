// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.memory_leaks;

import org.chromium.base.test.util.LeakCanaryChecker.LeakCanaryConfigProvider;
import org.chromium.build.annotations.ServiceImpl;

import java.util.List;
import java.util.Map;

@ServiceImpl(LeakCanaryConfigProvider.class)
public class ChromeInstrumentationLeaks implements LeakCanaryConfigProvider {
    // This class is a collection of known leaks in Chrome Instrumentation tests (eg.
    // chrome_public_test_apk). The goal is to  burn this class down to nothing by fixing leaks.
    // Please include a bug for each leak.

    @Override
    public Map<String, String> getStaticFieldLeaks() {
        return Map.of();
    }

    @Override
    public List<String> getJavaLocalLeaks() {
        return List.of();
    }
}
