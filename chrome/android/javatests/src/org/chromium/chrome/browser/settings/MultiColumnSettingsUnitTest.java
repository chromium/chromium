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
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
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
import org.chromium.base.ObserverList;
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
import org.chromium.base.test.util.CriteriaHelper;
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
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.BlankUiTestActivity;

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
        ObserverList<MultiColumnSettings.Observer> observers = new ObserverList<>();
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
            // Populate screen with a placeholder preference header targeting TestFragment.
            // PreferenceHeaderFragmentCompat.onCreateInitialDetailFragment() inspects category 0
            // in this screen to determine and instantiate the default detail pane fragment.
            var screen = getPreferenceManager().createPreferenceScreen(requireContext());
            var pref = new Preference(requireContext());
            pref.setFragment(TestFragment.class.getName());
            screen.addPreference(pref);
            setPreferenceScreen(screen);
        }
    }

    public static class TestMultiColumnSettings extends MultiColumnSettings {
        private final MainSettings mMainSettings = new TestMainSettings();
        private Fragment mInitialDetailFragment;
        private boolean mInitialDetailFragmentCreated;
        private @Nullable Boolean mIsTwoColumnForTesting;

        void setIsTwoColumnForTesting(@Nullable Boolean isTwoColumn) {
            mIsTwoColumnForTesting = isTwoColumn;
        }

        @Override
        boolean isTwoColumn() {
            if (mIsTwoColumnForTesting != null) {
                return mIsTwoColumnForTesting;
            }
            return super.isTwoColumn();
        }

        @Override
        public PreferenceFragmentCompat onCreatePreferenceHeader() {
            return mMainSettings;
        }

        @Override
        public Fragment onCreateInitialDetailFragment() {
            if (!mInitialDetailFragmentCreated) {
                // Note that this may legitimately return null.
                mInitialDetailFragment = super.onCreateInitialDetailFragment();
                mInitialDetailFragmentCreated = true;
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
                    // Fragment.instantiate) carries R.style.ThemeOverlay_Chromium_Settings by
                    // checking that preferenceTheme resolves.
                    Context context = settings.requireContext();
                    TypedValue tv = new TypedValue();
                    assertTrue(
                            "Theme should resolve preferenceTheme attribute from"
                                    + " ThemeOverlay_Chromium_Settings",
                            context.getTheme().resolveAttribute(R.attr.preferenceTheme, tv, true));
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnCreateInitialDetailFragment_SettingsInTab_TwoColumnMode() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setIsTwoColumnForTesting(true);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    assertNotNull(
                            "In two-column mode, onCreateInitialDetailFragment should return"
                                    + " default detail fragment",
                            settings.onCreateInitialDetailFragment());
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnCreateInitialDetailFragment_SettingsInTab_SingleColumnMode() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setIsTwoColumnForTesting(false);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    assertNull(
                            "In single-column mode, onCreateInitialDetailFragment should return"
                                    + " null",
                            settings.onCreateInitialDetailFragment());
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testUpdateHeaderPaneFocusability() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setIsTwoColumnForTesting(false);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    ViewGroup headerGroup =
                            settings.requireView().findViewById(R.id.preferences_header);
                    assertNotNull(headerGroup);

                    // In single-column mode with detail pane open, header descendants are blocked.
                    settings.showDetailFragment(new TestFragment(), false, null);
                    assertEquals(
                            ViewGroup.FOCUS_BLOCK_DESCENDANTS,
                            headerGroup.getDescendantFocusability());
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                            headerGroup.getImportantForAccessibility());

                    // In two-column mode, header descendants are allowed.
                    settings.setIsTwoColumnForTesting(true);
                    settings.updateHeaderPaneFocusability();
                    assertEquals(
                            ViewGroup.FOCUS_AFTER_DESCENDANTS,
                            headerGroup.getDescendantFocusability());
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                            headerGroup.getImportantForAccessibility());
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testEmptyBackStack_InTwoColumnMode_EnsuresInitialDetailFragment() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setIsTwoColumnForTesting(true);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    // Open a detail fragment with addToBackStack=true.
                    settings.showDetailFragment(new TestFragment(), true, null);
                    settings.getChildFragmentManager().executePendingTransactions();
                    assertEquals(1, settings.getChildFragmentManager().getBackStackEntryCount());

                    // Pop the back stack so entry count becomes 0.
                    settings.getChildFragmentManager().popBackStackImmediate();
                    assertEquals(0, settings.getChildFragmentManager().getBackStackEntryCount());

                    // The initial detail fragment should be re-populated in two-column mode.
                    assertNotNull(
                            "Detail fragment should be present in two-column mode when back stack"
                                    + " is empty",
                            settings.getChildFragmentManager()
                                    .findFragmentById(R.id.preferences_detail));
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testEmptyBackStack_InSingleColumnMode_ClosesSlidingPane() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        final TestMultiColumnSettings[] settingsHolder = new TestMultiColumnSettings[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
                    int narrowWidth =
                            (int)
                                    TypedValue.applyDimension(
                                            TypedValue.COMPLEX_UNIT_DIP, 400, metrics);
                    FrameLayout container = new FrameLayout(activity);
                    container.setId(View.generateViewId());
                    activity.setContentView(
                            container,
                            new ViewGroup.LayoutParams(
                                    narrowWidth, ViewGroup.LayoutParams.MATCH_PARENT));

                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settingsHolder[0] = settings;
                    settings.setIsTwoColumnForTesting(false);

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(container.getId(), settings)
                            .commitNow();
                });

        // Wait for the layout and measure pass to complete so SlidingPaneLayout evaluates
        // isSlideable() with the narrow container width.
        CriteriaHelper.pollUiThread(
                () -> settingsHolder[0].getSlidingPaneLayout().isSlideable(),
                "SlidingPaneLayout should become slideable with narrow container");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = settingsHolder[0];
                    // Open a detail fragment with addToBackStack=true.
                    settings.showDetailFragment(new TestFragment(), true, null);
                    settings.getChildFragmentManager().executePendingTransactions();
                });

        // Wait for the detail pane to slide open.
        CriteriaHelper.pollUiThread(
                () -> settingsHolder[0].getSlidingPaneLayout().isOpen(),
                "SlidingPaneLayout should open when detail fragment is shown");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            1,
                            settingsHolder[0].getChildFragmentManager().getBackStackEntryCount());
                    // Pop the back stack so entry count becomes 0.
                    settingsHolder[0].getChildFragmentManager().popBackStackImmediate();
                    assertEquals(
                            0,
                            settingsHolder[0].getChildFragmentManager().getBackStackEntryCount());
                });

        // In single-column mode, SlidingPaneLayout should close when detail is popped.
        CriteriaHelper.pollUiThread(
                () -> !settingsHolder[0].getSlidingPaneLayout().isOpen(),
                "SlidingPaneLayout closes when detail fragment is popped in single-column mode");
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    public void testOnCreateInitialDetailFragment_withInitialUrl() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestMultiColumnSettings settings = new TestMultiColumnSettings();
                    settings.setInitialUrl(
                            "chrome://settings/siteDetails?site=https%3A%2F%2Fgoogle.com");

                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Fragment detailFragment = settings.onCreateInitialDetailFragment();
                    assertNotNull(
                            "Detail fragment should be instantiated for initial subpage URL",
                            detailFragment);
                    assertTrue(
                            "Detail fragment should be SingleWebsiteSettings instance",
                            detailFragment instanceof SingleWebsiteSettings);
                    assertNotNull(detailFragment.getArguments());
                    assertEquals(
                            "https://google.com",
                            detailFragment
                                    .getArguments()
                                    .getString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS));
                    assertNull(
                            "Initial URL should be cleared after being consumed",
                            settings.getInitialUrl());
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    public void testOnPreferenceStartFragment_delegatesToSettingsNavigation() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsNavigation mockNavigation = Mockito.mock(SettingsNavigation.class);
                    SettingsNavigationFactory.setInstanceForTesting(mockNavigation);

                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Preference preference = new Preference(settings.requireContext());
                    preference.setFragment(TestFragment.class.getName());
                    Bundle extras = preference.getExtras();
                    extras.putString("test_key", "test_value");

                    PreferenceFragmentCompat caller = Mockito.mock(PreferenceFragmentCompat.class);
                    boolean handled = settings.onPreferenceStartFragment(caller, preference);

                    assertTrue("Preference start fragment should be handled", handled);
                    Mockito.verify(mockNavigation)
                            .startSettings(settings.getContext(), TestFragment.class, extras);
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    public void testOnPreferenceStartFragment_urlNavDisabled_fallsBackToParent() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsNavigation mockNavigation = Mockito.mock(SettingsNavigation.class);
                    SettingsNavigationFactory.setInstanceForTesting(mockNavigation);

                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Preference preference = new Preference(settings.requireContext());
                    preference.setFragment(TestFragment.class.getName());

                    PreferenceFragmentCompat caller = Mockito.mock(PreferenceFragmentCompat.class);
                    settings.onPreferenceStartFragment(caller, preference);

                    Mockito.verifyNoInteractions(mockNavigation);
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    public void testOnPreferenceStartFragment_nullFragment_fallsBackToParent() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsNavigation mockNavigation = Mockito.mock(SettingsNavigation.class);
                    SettingsNavigationFactory.setInstanceForTesting(mockNavigation);

                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Preference preference = new Preference(settings.requireContext());
                    preference.setFragment(null);

                    PreferenceFragmentCompat caller = Mockito.mock(PreferenceFragmentCompat.class);
                    boolean handled = settings.onPreferenceStartFragment(caller, preference);

                    assertFalse("Null fragment should not be handled by URL navigation", handled);
                    Mockito.verifyNoInteractions(mockNavigation);
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    public void testOnPreferenceStartFragment_invalidFragmentClass_fallsBackToParent() {
        mBlankUiActivityTestRule.launchActivity(null);
        BlankUiTestActivity activity = mBlankUiActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsNavigation mockNavigation = Mockito.mock(SettingsNavigation.class);
                    SettingsNavigationFactory.setInstanceForTesting(mockNavigation);

                    MultiColumnSettings settings = new TestMultiColumnSettings();
                    activity.getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, settings)
                            .commitNow();

                    Preference preference = new Preference(settings.requireContext());
                    preference.setFragment("invalid.fragment.class.Name");

                    PreferenceFragmentCompat caller = Mockito.mock(PreferenceFragmentCompat.class);
                    settings.onPreferenceStartFragment(caller, preference);

                    Mockito.verifyNoInteractions(mockNavigation);
                });
    }

    private SettingsActivityInterface startSettings() {
        return mSettingsTestRule.startSettingsActivity();
    }
}
