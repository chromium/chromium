// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorShareAction.TabListEditorShareActionState;

/** Unit tests for {@link TabUiMetricsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiMetricsHelperUnitTest {
    @Test
    public void testShareStateHistogram() {
        String histogramName = "Android.TabMultiSelectV2.SharingState";
        TabUiMetricsHelper.recordShareStateHistogram(TabListEditorShareActionState.SUCCESS);
        assertThat(RecordHistogram.getHistogramValueCountForTesting(histogramName, 1), equalTo(1));
    }
}
