// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

import java.util.List;

/** Unit tests for {@link TabbedAdaptiveToolbarBehavior}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedAdaptiveToolbarBehaviorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask mActorTask;

    private TabbedAdaptiveToolbarBehavior mBehavior;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(Activity.class);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        mBehavior =
                new TabbedAdaptiveToolbarBehavior(
                        activity,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null,
                        () -> mTabModelSelector,
                        null,
                        null,
                        null,
                        null);
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testResultFilterWithGlicEnabled() {
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mActorTask);

        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.SHARE, AdaptiveToolbarButtonVariant.GLIC),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.GLIC);
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
