// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.settings.SettingsActivityUnitTest.ShadowProfileManagerUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedDividerItemDecoration;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link SettingsActivity}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowProfileManagerUtils.class)
public class SettingsActivityUnitTest {
    /** Shadow class to bypass the real call to ProfileManagerUtils. */
    @Implements(ProfileManagerUtils.class)
    public static class ShadowProfileManagerUtils {
        @Implementation
        protected static void flushPersistentDataForAllProfiles() {}
    }

    @Rule
    public Features.JUnitProcessor featuresRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mockitoRule = MockitoJUnit.rule();

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;

    @Mock
    public ChromeBrowserInitializer mInitializer;
    @Mock
    public Profile mProfile;

    @Before
    public void setup() {
        ChromeBrowserInitializer.setForTesting(mInitializer);
        Profile.setLastUsedProfileForTesting(mProfile);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
            mActivityScenario = null;
        }
        ChromeBrowserInitializer.setForTesting(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testDefaultLaunchProcess() {
        launchSettingsActivity(TestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);

        assertTrue("SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestSettingsFragment);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR_ACTIVITY)
    public void testBackPress() throws TimeoutException {
        launchSettingsActivity(TestSettingsFragment.class.getName());
        assertTrue("SettingsActivity is using a wrong fragment.",
                mSettingsActivity.getMainFragment() instanceof TestSettingsFragment);
        TestSettingsFragment mainFragment =
                (TestSettingsFragment) mSettingsActivity.getMainFragment();
        mainFragment.getHandleBackPressChangedSupplier().set(true);
        Assert.assertTrue("TestSettingsFragment will handle back press",
                mSettingsActivity.getOnBackPressedDispatcher().hasEnabledCallbacks());

        // Simulate back press.
        TestThreadUtils.runOnUiThreadBlocking(
                mSettingsActivity.getOnBackPressedDispatcher()::onBackPressed);
        mainFragment.getBackPressCallback().waitForFirst();

        mainFragment.getHandleBackPressChangedSupplier().set(false);
        Assert.assertFalse("TestSettingsFragment will not handle back press",
                mSettingsActivity.getOnBackPressedDispatcher().hasEnabledCallbacks());
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay() {
        launchSettingsActivity(TestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedDividerItemDecoration decoration = getPaddedDividerDecoration(recyclerView);
        assertNotNull("PaddedDividerItemDecoration should exists.", decoration);
        int parentPadding =
                60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2
        assertEquals("Divider start padding is wrong.", parentPadding,
                decoration.getDividerPaddingStart());
        assertEquals(
                "Divider end padding is wrong.", parentPadding, decoration.getDividerPaddingEnd());
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay_NoDivider() {
        CustomDividerTestSettingsFragment.sHasDivider = false;
        launchSettingsActivity(CustomDividerTestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        int padding = 60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2
        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        assertEquals("Padding start is wrong.", padding, recyclerView.getPaddingStart());
        assertEquals("Padding end is wrong.", padding, recyclerView.getPaddingEnd());

        assertNull("PaddedDividerItemDecoration should not exist when no divider is in use.",
                getPaddedDividerDecoration(recyclerView));
    }

    @Test
    @Config(qualifiers = "w720dp-h1024dp")
    public void addPaddingToContentOnWideDisplay_HasCustomDivider() {
        CustomDividerTestSettingsFragment.sHasDivider = true;

        launchSettingsActivity(CustomDividerTestSettingsFragment.class.getName());
        mActivityScenario.moveToState(State.CREATED);
        mActivityScenario.moveToState(State.STARTED);
        mActivityScenario.moveToState(State.RESUMED);

        RecyclerView recyclerView = mSettingsActivity.findViewById(R.id.recycler_view);
        PaddedDividerItemDecoration decoration = getPaddedDividerDecoration(recyclerView);
        assertNotNull("PaddedDividerItemDecoration should exists.", decoration);
        int parentPadding =
                60; // (720 - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2 = (720 - 600) / 2
        assertEquals("Divider start padding is wrong.",
                parentPadding + CustomDividerTestSettingsFragment.DIVIDER_START_PADDING,
                decoration.getDividerPaddingStart());
        assertEquals("Divider end padding is wrong.",
                parentPadding + CustomDividerTestSettingsFragment.DIVIDER_END_PADDING,
                decoration.getDividerPaddingEnd());
    }

    private void launchSettingsActivity(String fragmentName) {
        assert mActivityScenario == null : "Should be called once per test.";
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), SettingsActivity.class);
        intent.putExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT, fragmentName);
        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(activity -> mSettingsActivity = activity);
    }

    private PaddedDividerItemDecoration getPaddedDividerDecoration(RecyclerView recyclerView) {
        for (int i = 0; i < recyclerView.getItemDecorationCount(); ++i) {
            if (recyclerView.getItemDecorationAt(i) instanceof PaddedDividerItemDecoration) {
                return (PaddedDividerItemDecoration) recyclerView.getItemDecorationAt(i);
            }
        }
        return null;
    }

    /**
     * Class that override the divider behavior.
     */
    public static class CustomDividerTestSettingsFragment
            extends TestSettingsFragment implements CustomDividerFragment {
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
