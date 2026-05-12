// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
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

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

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
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    @Mock private Runnable mRegisterVoiceSearchRunnable;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabBookmarker mTabBookmarker;
    @Mock private GroupSuggestionsButtonController mGroupSuggestionsButtonController;
    @Mock private GlicButtonDelegate mGlicButtonDelegate;
    @Mock private ChromeAndroidTask mChromeAndroidTask;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private final MonotonicObservableSupplier<Integer> mTabStripVisibilitySupplier =
            ObservableSuppliers.createMonotonic();
    private final NullableObservableSupplier<BookmarkModel> mBookmarkModelSupplier =
            ObservableSuppliers.createNullable();

    private TabbedAdaptiveToolbarBehavior mBehavior;

    @Before
    public void setUp() {
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        Activity activity = Robolectric.setupActivity(Activity.class);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        mBehavior =
                new TabbedAdaptiveToolbarBehavior(
                        activity,
                        mActivityLifecycleDispatcher,
                        () -> mTabCreatorManager,
                        () -> mTabBookmarker,
                        mBookmarkModelSupplier,
                        mActivityTabProvider,
                        mRegisterVoiceSearchRunnable,
                        () -> mGroupSuggestionsButtonController,
                        () -> mTabModelSelector,
                        mTabStripVisibilitySupplier,
                        mGlicButtonDelegate,
                        () -> mChromeAndroidTask,
                        mBrowserControlsVisibilityManager);
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testResultFilterWithGlicEnabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(eq(mProfile))).thenReturn(true);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorKeyedService.getActiveTasks()).thenReturn(List.of(mActorTask));
        mBehavior.registerPerSurfaceButtons(mAdaptiveToolbarButtonController, () -> null);
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
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testResultFilter_BottomBarEnabled() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
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

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testCanShowManualOverride_BottomBarEnabled() {
        assertFalse(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.GLIC));
        assertFalse(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.NEW_TAB));
        assertTrue(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.SHARE));
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testCanShowManualOverride_BottomBarDisabled() {
        assertTrue(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.GLIC));
        assertTrue(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.NEW_TAB));
        assertTrue(mBehavior.canShowManualOverride(AdaptiveToolbarButtonVariant.SHARE));
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ANDROID_BOTTOM_BAR})
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testRegisterPerSurfaceButtons_BottomBarEnabled() {
        mBehavior.registerPerSurfaceButtons(mAdaptiveToolbarButtonController, () -> null);
        verify(mAdaptiveToolbarButtonController, never())
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.NEW_TAB), any());
        verify(mAdaptiveToolbarButtonController, never())
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.GLIC), any());
        verify(mAdaptiveToolbarButtonController)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS), any());
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures({
        ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
        ChromeFeatureList.ANDROID_BOTTOM_BAR
    })
    public void testRegisterPerSurfaceButtons_BottomBarDisabled() {
        mBehavior.registerPerSurfaceButtons(mAdaptiveToolbarButtonController, () -> null);
        verify(mAdaptiveToolbarButtonController)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.NEW_TAB), any());
        verify(mAdaptiveToolbarButtonController)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.GLIC), any());
        verify(mAdaptiveToolbarButtonController)
                .addButtonVariant(eq(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS), any());
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
