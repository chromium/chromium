// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;

import androidx.lifecycle.Lifecycle.State;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ActivityScenario;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.ui.display.DisplayUtil;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link SettingsActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.SETTINGS_IN_TAB, // crbug.com/521895796
    ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP // crbug.com/556881398
})
@EnableFeatures({ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES})
public class SettingsActivityUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;

    @Mock public ChromeBrowserInitializer mInitializer;
    @Mock public Profile mProfile;
    @Mock public ActorKeyedService mActorKeyedService;

    @Before
    public void setup() {
        ProfileManagerUtils.setFlushPersistentDataCallbackForTesting(() -> {});
        ChromeBrowserInitializer.setForTesting(mInitializer);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
            mActivityScenario = null;
        }
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void testApplyOverrides() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        assertEquals(
                "SmallestScreenWidthDp should be overridden.",
                720,
                mSettingsActivity.getResources().getConfiguration().smallestScreenWidthDp);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE})
    public void testAutomotiveBackButtonBarStreamline_hidesToolbarOnStart() {
        // Required for the feature flag check to pass.
        DisplayUtil.setCarmaPhase1Version2ComplianceForTesting(true);
        DeviceInfo.setIsAutomotiveForTesting(true);

        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        View backButtonToolbar = mSettingsActivity.findViewById(R.id.back_button_toolbar);
        assertNotNull("The back button toolbar should exist in the xml layout.", backButtonToolbar);
        assertEquals(
                "The back button toolbar should be gone when the settings page is opened.",
                View.GONE,
                backButtonToolbar.getVisibility());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testDefaultLaunchProcess() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestEmbeddableFragment);
        assertNotNull(mSettingsActivity.getIntentRequestTracker());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testDefaultLaunchProcessSingleActivity() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestEmbeddableFragment);
        assertNotNull(mSettingsActivity.getIntentRequestTracker());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testUpdateTitle() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.RESUMED);

        assertEquals("Activity title is not set.", "test title", mSettingsActivity.getTitle());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testUpdateTitleSingleActivity() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.RESUMED);

        // Simulate opening a new fragment.
        Bundle args = new Bundle();
        args.putString(TestEmbeddableFragment.EXTRA_TITLE, "new title");
        Intent intent =
                SettingsIntentUtil.createIntent(
                        mSettingsActivity, TestEmbeddableFragment.class.getName(), args);

        // Android temporarily pauses an activity while delivering a new intent.
        mActivityScenario.moveToState(State.STARTED);
        mSettingsActivity.onNewIntent(intent);
        mActivityScenario.moveToState(State.RESUMED);

        // Wait for the UI update.
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals("Activity title is not updated.", "new title", mSettingsActivity.getTitle());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testIntentFlags() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.RESUMED);

        Intent embeddableFragmentIntent =
                SettingsIntentUtil.createIntent(
                        mSettingsActivity, TestEmbeddableFragment.class.getName(), null);
        assertEquals(
                "Incorrect intent flags for embeddable fragments",
                Intent.FLAG_ACTIVITY_SINGLE_TOP,
                embeddableFragmentIntent.getFlags());

        Intent standaloneFragmentIntent =
                SettingsIntentUtil.createIntent(
                        mSettingsActivity, TestStandaloneFragment.class.getName(), null);
        assertEquals(
                "Incorrect intent flags for standalone fragments",
                0,
                standaloneFragmentIntent.getFlags());
    }

    @Test
    public void testBackPress() throws TimeoutException {
        startSettingsActivity(TestStandaloneFragment.class.getName());
        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestStandaloneFragment);
        TestStandaloneFragment mainFragment =
                (TestStandaloneFragment) mSettingsActivity.getMainFragment();
        mainFragment.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue(
                "TestStandaloneFragment will handle back press",
                mSettingsActivity.getOnBackPressedDispatcher().hasEnabledCallbacks());

        // Simulate back press.
        ThreadUtils.runOnUiThreadBlocking(
                mSettingsActivity.getOnBackPressedDispatcher()::onBackPressed);
        mainFragment.getBackPressCallback().waitForOnly();

        mainFragment.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse(
                "TestStandaloneFragment will not handle back press",
                mSettingsActivity.getOnBackPressedDispatcher().hasEnabledCallbacks());
    }

    @Test
    public void testEscapeKey() throws TimeoutException {
        startSettingsActivity(TestStandaloneFragment.class.getName());
        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestStandaloneFragment);
        assertFalse(mSettingsActivity.isFinishing());

        // Simulate escape key press.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
                    assertTrue(mSettingsActivity.dispatchKeyEvent(event));
                });
        assertTrue(mSettingsActivity.isFinishing());
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedItemDecorationWithDivider decoration = getPaddedDecoration(recyclerView);
        assertNotNull("PaddedItemDecorationWithDivider should exists.", decoration);
        int parentPadding =
                60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2
        int itemOffset = decoration.getItemOffsetForTesting();
        assertEquals("Item offset is wrong.", parentPadding, itemOffset);
        assertEquals("Divider start padding is wrong.", 0, decoration.getDividerPaddingStart());
        assertEquals("Divider end padding is wrong.", 0, decoration.getDividerPaddingEnd());
    }

    @Test
    @Config(qualifiers = "w320dp-h1024dp")
    public void addPaddingToContentOnNarrowDisplay() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedItemDecorationWithDivider decoration = getPaddedDecoration(recyclerView);
        assertNotNull("PaddedItemDecorationWithDivider should exists.", decoration);
        int itemOffset = decoration.getItemOffsetForTesting();
        assertEquals("Item offset is wrong.", 0, itemOffset);
        assertEquals("Divider start padding is wrong.", 0, decoration.getDividerPaddingStart());
        assertEquals("Divider end padding is wrong.", 0, decoration.getDividerPaddingEnd());
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay_NoDivider() {
        CustomDividerTestSettingsFragment.sHasDivider = false;
        startSettingsActivity(CustomDividerTestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedItemDecorationWithDivider decoration = getPaddedDecoration(recyclerView);
        assertNotNull("PaddedItemDecorationWithDivider should exists.", decoration);
        int parentPadding =
                60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2

        int itemOffset = decoration.getItemOffsetForTesting();
        assertEquals("Item offset is wrong.", parentPadding, itemOffset);
        assertEquals(
                "Divider start padding should not be set.", 0, decoration.getDividerPaddingStart());
        assertEquals(
                "Divider end padding should not be set.", 0, decoration.getDividerPaddingEnd());
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay_HasCustomDivider() {
        CustomDividerTestSettingsFragment.sHasDivider = true;

        startSettingsActivity(CustomDividerTestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedItemDecorationWithDivider decoration = getPaddedDecoration(recyclerView);

        assertNotNull("PaddedItemDecorationWithDivider should exists.", decoration);
        int parentPadding =
                60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2
        int itemOffset = decoration.getItemOffsetForTesting();
        assertEquals("Item offset is wrong.", parentPadding, itemOffset);
        assertEquals(
                "Divider start padding is wrong.",
                CustomDividerTestSettingsFragment.DIVIDER_START_PADDING,
                decoration.getDividerPaddingStart());
        assertEquals(
                "Divider end padding is wrong.",
                CustomDividerTestSettingsFragment.DIVIDER_END_PADDING,
                decoration.getDividerPaddingEnd());

        // simulate onDraw() call and verify padding
        RecyclerView.State state = new RecyclerView.State();
        decoration.onDraw(new Canvas(), recyclerView, state);
        for (int index = 0; index < recyclerView.getChildCount(); index++) {
            View view = recyclerView.getChildAt(index);
            Rect outRect = new Rect();
            decoration.getItemOffsets(outRect, view, recyclerView, state);
            assertEquals("Recycler view item offset padding is wrong", parentPadding, outRect.left);
            assertEquals("Recycler view item end offset is wrong", parentPadding, outRect.right);
        }
    }

    @Test
    public void testEscapeKey_HandledByFragment() throws TimeoutException {
        startSettingsActivity(TestStandaloneFragment.class.getName());
        TestStandaloneFragment mainFragment =
                (TestStandaloneFragment) mSettingsActivity.getMainFragment();
        mainFragment.getHandleBackPressChangedSupplier().set(true);
        assertTrue(mSettingsActivity.getOnBackPressedDispatcher().hasEnabledCallbacks());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
                    assertTrue(mSettingsActivity.dispatchKeyEvent(event));
                });

        // Check that the back press was triggered. More of a confidence check.
        mainFragment.getBackPressCallback().waitForOnly();

        // Check that #finish was not triggered, to verify it went down the path of escape handling.
        assertFalse(
                "Finishing the activity should not have been triggered with a handler ready to act"
                    + " on the event.",
                mSettingsActivity.isFinishing());
    }

    @Test
    public void testOnConfigurationChanged_updatesContainment() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        SettingsContainmentHelper mockHelper = mock(SettingsContainmentHelper.class);
        mSettingsActivity.setContainmentHelperForTesting(mockHelper);
        mSettingsActivity.setMultiColumnSettingsForTesting(mock(MultiColumnSettings.class));

        mSettingsActivity.onConfigurationChanged(new Configuration());

        verify(mockHelper).updateContainmentForAttachedFragments(any());
    }

    @Test
    public void testOnHeaderLayoutUpdated_updatesContainment() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        SettingsContainmentHelper mockHelper = mock(SettingsContainmentHelper.class);
        mSettingsActivity.setContainmentHelperForTesting(mockHelper);

        mSettingsActivity.onHeaderLayoutUpdated();

        verify(mockHelper).updateContainmentForAttachedFragments(any());
    }

    /**
     * Settings hosted by this activity must keep navigating within the activity, even when the
     * global SettingsInTab state changes to "open in a tab" after settings was opened (e.g. a
     * foldable device was unfolded). See crbug.com/562619494.
     */
    @Test
    public void testStartSettings_StaysInActivityWhenGlobalStateSaysTab() {
        startSettingsActivity(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        // Simulate the window becoming wide enough to open settings in a tab while this activity
        // is already showing settings.
        SettingsInTab.setShouldOpenSettingsInTabForTesting(true);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSettingsActivity.startSettings(
                                TestEmbeddableFragment.class.getName(), /* args= */ null));

        // Robolectric records the Intent passed to startActivity() instead of launching it, so no
        // second SettingsActivity is created here. Only the Intent target is checked. In
        // production the Intent carries FLAG_ACTIVITY_SINGLE_TOP when SettingsSingleActivity is
        // enabled, so it is delivered to this activity's onNewIntent().
        Intent startedIntent = shadowOf(mSettingsActivity).getNextStartedActivity();
        assertNotNull("startSettings() should have sent an Intent.", startedIntent);
        assertEquals(
                "Navigation from activity-hosted settings must target SettingsActivity, not a tab.",
                SettingsActivity.class.getName(),
                startedIntent.getComponent().getClassName());
    }

    private void startSettingsActivity(String fragmentName) {
        assertWithMessage("Should be called once per test.").that(mActivityScenario).isNull();
        Intent intent =
                SettingsIntentUtil.createIntent(
                        ContextUtils.getApplicationContext(), fragmentName, null);
        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(activity -> mSettingsActivity = activity);
    }

    private PaddedItemDecorationWithDivider getPaddedDecoration(RecyclerView recyclerView) {
        for (int i = 0; i < recyclerView.getItemDecorationCount(); ++i) {
            if (recyclerView.getItemDecorationAt(i) instanceof PaddedItemDecorationWithDivider) {
                return (PaddedItemDecorationWithDivider) recyclerView.getItemDecorationAt(i);
            }
        }
        return null;
    }

    /** Class that override the divider behavior. */
    public static class CustomDividerTestSettingsFragment extends TestEmbeddableFragment
            implements CustomDividerFragment {
        static final int DIVIDER_START_PADDING = 10;
        static final int DIVIDER_END_PADDING = 15;

        public static boolean sHasDivider;

        public CustomDividerTestSettingsFragment() {}

        @Override
        public boolean hasDivider() {
            return sHasDivider;
        }

        @Override
        public int getDividerStartPadding() {
            return DIVIDER_START_PADDING;
        }

        @Override
        public int getDividerEndPadding() {
            return DIVIDER_END_PADDING;
        }
    }
}
