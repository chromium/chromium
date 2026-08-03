// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.util.TypedValue;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@Batch(Batch.PER_CLASS)
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class MultiColumnSettingsUnitTest {

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("AccountManagerFacadeSource"),
                    new ParameterSet().value(true).name("IdentityManagerSource"));

    @Rule
    public SettingsTestRule<MainSettings> mSettingsTestRule =
            new SettingsTestRule<>(MainSettings.class);

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mBlankUiActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @After
    public void tearDown() {
        if (mSettingsTestRule.getActivity() != null) {
            mSettingsTestRule.getActivity().finish();
        }
        if (mBlankUiActivityTestRule.getActivity() != null) {
            mBlankUiActivityTestRule.getActivity().finish();
        }
    }

    // Hack to trick the test target about back stack entries.
    // In this test, count is enough.
    private static class TestFragmentManager extends FragmentManager {
        private int mBackStackCount;

        void addBackStack() {
            ++mBackStackCount;
        }

        void removeBackStack() {
            ++mBackStackCount;
        }

        void clearBackStack() {
            mBackStackCount = 0;
        }

        @Override
        public int getBackStackEntryCount() {
            return mBackStackCount;
        }
    }

    // Stub fragment instance of EmbeddableSettingsPage providing a fake page title instance.
    @UsedByReflection("MultiColumnSettingsUnitTest.java")
    public static class TestFragment extends Fragment implements EmbeddableSettingsPage {
        // Tests use reference equality to test for different fragments, so cannot use
        // ObservableSuppliers.alwaysNull().
        private final MonotonicObservableSupplier<String> mTitleSupplier =
                ObservableSuppliers.createMonotonic();

        @UsedByReflection("MultiColumnSettingsUnitTest.java")
        public TestFragment() {}

        @Override
        public MonotonicObservableSupplier<String> getPageTitle() {
            return mTitleSupplier;
        }

        @Override
        public @AnimationType int getAnimationType() {
            return AnimationType.PROPERTY;
        }
    }

    private final boolean mIsIdentityManagerSourceOfAccounts;

    public MultiColumnSettingsUnitTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);

        SigninManager mockSigninManager = Mockito.mock(SigninManager.class);
        TemplateUrlService mockTemplateUrlService = Mockito.mock(TemplateUrlService.class);
        SyncService mockSyncService = Mockito.mock(SyncService.class);

        IdentityServicesProvider.setSigninManagerForTesting(mockSigninManager);
        TemplateUrlServiceFactory.setInstanceForTesting(mockTemplateUrlService);
        SyncServiceFactory.setInstanceForTesting(mockSyncService);

        ResettersForTesting.register(
                () -> Mockito.reset(mockSigninManager, mockTemplateUrlService, mockSyncService));
    }

    // Creation of fragments (specifically, ObservableSupplierImpl) requires
    // to run on non instrumentation thread.
    @Test
    @SmallTest
    @UiThreadTest
    public void testFragmentTracker() {
        List<MultiColumnSettings.Observer> observers = new ArrayList<>();
        var fragmentManager = new TestFragmentManager();

        var fragmentTracker = new MultiColumnSettings.FragmentTracker(observers);
        assertEquals(0, fragmentTracker.mTitles.size());

        // Simulate loading the main page at the left pane.
        var mainFragment = new MainSettings();
        fragmentTracker.onFragmentResumed(fragmentManager, mainFragment);
        // Detailed paget title should be not affected.
        assertEquals(0, fragmentTracker.mTitles.size());

        // Load a detailed page.
        var fragment1 = new TestFragment();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment1);

        assertEquals(1, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);
        }

        // Load another detailed page.
        var fragment2 = new TestFragment();
        fragmentManager.addBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment2);

        assertEquals(2, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);
        }

        // Load yet another detailed page.
        var fragment3 = new TestFragment();
        fragmentManager.addBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment3);

        assertEquals(3, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);

            var title3 = fragmentTracker.mTitles.get(2);
            assertSame(fragment3.getPageTitle(), title3.titleSupplier);
            assertEquals(2, title3.backStackCount);
        }

        // Restart the second fragment. The stack should be shrunk.
        fragmentManager.removeBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment2);

        assertEquals(2, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);
        }

        // Emulation of the tap on a menu item in the main menu.
        var fragment4 = new TestFragment();
        fragmentManager.clearBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment4);

        assertEquals(1, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment4.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);
        }
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT,
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    })
    @DisableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2,
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSinglePane() {
        SettingsActivityInterface activity = startSettings();
        MultiColumnSettings settings = (MultiColumnSettings) activity.getMultiColumnSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Layout should be slideable (panes stacked) on phones",
                            settings.getSlidingPaneLayout().isSlideable());
                });
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    @EnableFeatures({
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    })
    @DisableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2,
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testTwoPane() {
        SettingsActivityInterface activity = startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        MultiColumnSettings settings = (MultiColumnSettings) activity.getMultiColumnSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Layout should NOT be slideable (panes side-by-side) on tablets",
                            settings.getSlidingPaneLayout().isSlideable());
                });
    }

    public static class TestMainSettings extends MainSettings {
        private final Profile mMockProfile = Mockito.mock(Profile.class);

        public TestMainSettings() {
            setProfile(mMockProfile);
            setSkipUpdatePreferencesForTesting(true);
        }

        @Override
        public Profile getProfile() {
            return mMockProfile;
        }

        @Override
        public void onCreatePreferences(
                @Nullable Bundle savedInstanceState, @Nullable String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(requireContext()));
        }
    }

    public static class TestMultiColumnSettings extends MultiColumnSettings {
        private final MainSettings mMainSettings = new TestMainSettings();
        private Fragment mInitialDetailFragment;

        @Override
        public PreferenceFragmentCompat onCreatePreferenceHeader() {
            return mMainSettings;
        }

        @Override
        public Fragment onCreateInitialDetailFragment() {
            if (mInitialDetailFragment == null) {
                mInitialDetailFragment = super.onCreateInitialDetailFragment();
            }
            return mInitialDetailFragment;
        }
    }

    @Test
    @SmallTest
    public void testProcessPendingFragmentIntent_ConsumesAndRemovesIntentExtras() {
        Intent intent = new Intent();
        intent.putExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT, TestFragment.class.getName());
        intent.putExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS, new Bundle());
        intent.putExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK, true);
        intent.putExtra(SettingsIntentUtil.EXTRA_FRAGMENT_TAG, "test_tag");

        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setPendingFragmentIntent(intent);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    settings.onCreateInitialDetailFragment();

                    assertFalse(
                            "EXTRA_SHOW_FRAGMENT should be consumed and removed from intent",
                            intent.hasExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
                    assertFalse(
                            "EXTRA_SHOW_FRAGMENT_ARGUMENTS should be consumed and removed from"
                                    + " intent",
                            intent.hasExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS));
                    assertFalse(
                            "EXTRA_ADD_TO_BACK_STACK should be consumed and removed from intent",
                            intent.hasExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK));
                    assertFalse(
                            "EXTRA_FRAGMENT_TAG should be consumed and removed from intent",
                            intent.hasExtra(SettingsIntentUtil.EXTRA_FRAGMENT_TAG));
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testProcessPendingFragmentIntent_MainSettings_ReturnsNull() {
        Intent intent = new Intent();
        intent.putExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT, MainSettings.class.getName());

        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setPendingFragmentIntent(intent);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    // Ensure we don't instantiate a second instance of MainSettings as a detail
                    // fragment.
                    assertNull(
                            "MainSettings intent should not instantiate a detail fragment",
                            settings.onCreateInitialDetailFragment());

                    // Verify that MainSettings is created as the header fragment.
                    var header = settings.onCreatePreferenceHeader();
                    assertNotNull("MainSettings header fragment should be created", header);
                    assertTrue(
                            "Header fragment should be instance of MainSettings",
                            header instanceof TestMainSettings);
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testProcessPendingFragmentIntent_SubpageFragment_InstantiatesWithThemedContext() {
        Intent intent = new Intent();
        intent.putExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT, TestFragment.class.getName());

        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setPendingFragmentIntent(intent);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Fragment detailFragment = settings.onCreateInitialDetailFragment();
                    assertNotNull("Detail fragment should be instantiated", detailFragment);
                    assertTrue(
                            "Detail fragment should be TestFragment instance",
                            detailFragment instanceof TestFragment);

                    // Verify that settings.requireContext() (which was passed to
                    // Fragment.instantiate) carries R.style.Theme_Chromium_Settings by checking
                    // that preferenceTheme resolves.
                    Context context = settings.requireContext();
                    TypedValue tv = new TypedValue();
                    assertTrue(
                            "Theme should resolve preferenceTheme attribute from"
                                    + " Theme_Chromium_Settings",
                            context.getTheme().resolveAttribute(R.attr.preferenceTheme, tv, true));
                });
    }

    private SettingsActivityInterface startSettings() {
        return mSettingsTestRule.startSettingsActivity();
    }
}
