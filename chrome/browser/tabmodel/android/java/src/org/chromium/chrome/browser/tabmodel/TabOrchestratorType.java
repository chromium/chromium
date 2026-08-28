// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Identifies the type of TabModelOrchestrator creating and managing a TabPersistentStore. */
// LINT.IfChange(TabOrchestratorType)
@IntDef({
    TabOrchestratorType.TABBED,
    TabOrchestratorType.CUSTOM,
    TabOrchestratorType.ARCHIVED,
    TabOrchestratorType.HEADLESS,
})
@Retention(RetentionPolicy.SOURCE)
@Target({ElementType.TYPE_USE})
@NullMarked
public @interface TabOrchestratorType {
    int TABBED = 0;
    int CUSTOM = 1;
    int ARCHIVED = 2;
    int HEADLESS = 3;
    int NUM_ENTRIES = 4;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:TabOrchestratorType)
