// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

import java.util.List;

/** Unit tests for {@link TabbedAdaptiveToolbarBehavior}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedAdaptiveToolbarBehaviorTest {
    private TabbedAdaptiveToolbarBehavior mBehavior;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        mBehavior =
                new TabbedAdaptiveToolbarBehavior(
                        activity, null, null, null, null, null, null, null, null, null, null);
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    public void testGetTopSegmentationResultOnPhone() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.NEW_TAB);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNTBOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNoNTBOrBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.TRANSLATE, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.TRANSLATE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithOnlyNTBAndBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.NEW_TAB),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.UNKNOWN);
    }

    private void assertTopResult(
            List<Integer> segmentationResults,
            @AdaptiveToolbarButtonVariant int expectedTopResult) {
        assertEquals(
                "Top segmentation result is not as expected.",
                expectedTopResult,
                mBehavior.resultFilter(segmentationResults));
    }
}
