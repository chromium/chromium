// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Bundle;
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
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.settings.SettingsActivityUnitTest.ShadowProfileManagerUtils;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link SettingsActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowProfileManagerUtils.class)
public class SettingsActivityUnitTest {
    /** Shadow class to bypass the real call to ProfileManagerUtils. */
    @Implements(ProfileManagerUtils.class)
    public static class ShadowProfileManagerUtils {
        @Implementation
        protected static void flushPersistentDataForAllProfiles() {}
    }

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;

    @Mock public ChromeBrowserInitializer mInitializer;
    @Mock public Profile mProfile;

    @Before
    public void setup() {
        ChromeBrowserInitializer.setForTesting(mInitializer);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
            mActivityScenario = null;
        }
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testDefaultLaunchProcess() {
        startSettings(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestEmbeddableFragment);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testDefaultLaunchProcessSingleActivity() {
        startSettings(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        assertTrue(
                "SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestEmbeddableFragment);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testUpdateTitle() {
        startSettings(TestEmbeddableFragment.class.getName());
        mActivityScenario.moveToState(State.RESUMED);

        assertEquals("Activity title is not set.", "test title", mSettingsActivity.getTitle());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testUpdateTitleSingleActivity() {
        startSettings(TestEmbeddableFragment.class.getName());
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
        ShadowLooper.runUiThreadTasks();

        assertEquals("Activity title is not updated.", "new title", mSettingsActivity.getTitle());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testIntentFlags() {
        startSettings(TestEmbeddableFragment.class.getName());
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
        startSettings(TestStandaloneFragment.class.getName());
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
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay() {
        startSettings(TestEmbeddableFragment.class.getName());
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
        startSettings(TestEmbeddableFragment.class.getName());
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
        startSettings(CustomDividerTestSettingsFragment.class.getName());
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

        startSettings(CustomDividerTestSettingsFragment.class.getName());
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

    private void startSettings(String fragmentName) {
        assert mActivityScenario == null : "Should be called once per test.";
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
