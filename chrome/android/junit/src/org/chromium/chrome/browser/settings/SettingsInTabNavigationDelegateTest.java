// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.page_info.PageInfoCookiesSettings;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SettingsInTabNavigationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsInTabNavigationDelegateTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mMockTab;
    private SettingsInTabNavigationDelegate mDelegate;

    /** Unmapped Fragment class for testing fallback behavior. */
    public static class UnmappedTestFragment extends Fragment {}

    @Before
    public void setUp() {
        mDelegate = new SettingsInTabNavigationDelegate(mMockTab);
    }

    @Test
    public void testStartSettings_Default() {
        // Tests startSettings(context) overload which implicitly loads the SettingsFragment.MAIN
        // fragment.
        mDelegate.startSettings(null);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals(UrlConstants.SETTINGS_URL, captor.getValue().getUrl());
    }

    @Test
    public void testStartSettings_ExplicitEnum() {
        // Tests startSettings(context, enum) resolves to the fragment class and canonical URL.
        mDelegate.startSettings(null, SettingsFragment.PRIVACY);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/privacy", captor.getValue().getUrl());
    }

    @Test
    public void testStartSettings_ExplictEnumAndBackstack() {
        // Tests startSettings(context, enum, addToBackStack) falls back to createSettingsIntent().
        Context mockContext = spy(ContextUtils.getApplicationContext());
        doNothing().when(mockContext).startActivity(any());

        mDelegate.startSettings(
                mockContext, SettingsFragment.PAGE_INFO_COOKIES, /* addToBackStack= */ true);

        // Verify URL load was not performed on the tab.
        verify(mMockTab, never()).loadUrl(any());

        // Verify that startSettings fell back to starting an intent on the context.
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(captor.capture());
        assertEquals(
                PageInfoCookiesSettings.class.getName(),
                captor.getValue().getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
        assertTrue(
                captor.getValue()
                        .getBooleanExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK, false));
    }

    @Test
    public void testStartSettings_ExplicitFragment() {
        // Tests startSettings(context, fragment) overload, which tests canonical url resolution
        // with a custom fragment.
        mDelegate.startSettings(null, ThemeSettingsFragment.class);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/theme", captor.getValue().getUrl());
    }

    @Test
    public void testStartSettings_NullFragment() {
        // Tests startSettings(context, null), which tests the behavior when no fragment is
        // specified.
        mDelegate.startSettings(null, null);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());

        // Expect that the default url (chrome://settings) is loaded as a fallback.
        assertEquals(UrlConstants.SETTINGS_URL, captor.getValue().getUrl());
    }

    @Test
    public void testStartSettings_UnmappedFragment_FallsBackToStartActivity() {
        Context mockContext = spy(ContextUtils.getApplicationContext());
        doNothing().when(mockContext).startActivity(any());

        mDelegate.startSettings(mockContext, UnmappedTestFragment.class);

        // Verify URL load was not performed on the tab.
        verify(mMockTab, never()).loadUrl(any());

        // Verify that startSettings fell back to starting an intent on the context.
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(captor.capture());
        assertEquals(
                UnmappedTestFragment.class.getName(),
                captor.getValue().getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
        assertTrue(
                captor.getValue()
                        .getBooleanExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_STANDALONE, false));
    }

    @Test
    public void testStartSettings_ExplicitFragmentAndBundleArgs() {
        // Tests startSettings(context, fragment, bundle) which tests loading a url with query
        // parameters.
        Bundle args = new Bundle();
        args.putString(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, "https://example.com");

        mDelegate.startSettings(null, SingleWebsiteSettings.class, args);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals(
                "chrome://settings/siteDetails?site=https%3A%2F%2Fexample.com",
                captor.getValue().getUrl());
    }

    @Test
    public void testStartSettings_ExplicitFragmentAndBundleArgsWithBackstack() {
        // Tests startSettings(context, fragment, bundle, addToBackStack) which tests loading a url
        // that has query parameters, but falls back to start settings from an Intent.
        Context mockContext = spy(ContextUtils.getApplicationContext());
        doNothing().when(mockContext).startActivity(any());

        Bundle args = new Bundle();
        args.putString("test_param", "test_value");

        mDelegate.startSettings(
                mockContext, UnmappedTestFragment.class, args, /* addToBackStack= */ true);

        // Verify URL load was not performed on the tab.
        verify(mMockTab, never()).loadUrl(any());

        // Verify that startSettings fell back to starting an intent on the context.
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(captor.capture());
        assertEquals(
                UnmappedTestFragment.class.getName(),
                captor.getValue().getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
        assertEquals(
                "test_value",
                captor.getValue()
                        .getBundleExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS)
                        .getString("test_param"));
        assertTrue(
                captor.getValue()
                        .getBooleanExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK, false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testCreateSettingsIntent_WithEnum_UrlNavDisabled() {
        Context context = ContextUtils.getApplicationContext();

        Intent intent =
                mDelegate.createSettingsIntent(
                        context, SettingsFragment.PRIVACY, /* fragmentArgs= */ null);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.SETTINGS_URL, intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                PrivacySettings.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testCreateSettingsIntent_WithEnum_UrlNavEnabled() {
        Context context = ContextUtils.getApplicationContext();

        Intent intent =
                mDelegate.createSettingsIntent(
                        context, SettingsFragment.PRIVACY, /* fragmentArgs= */ null);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals("chrome://settings/privacy", intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                PrivacySettings.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testCreateSettingsIntent_WithFragmentClass_UrlNavDisabled() {
        Context context = ContextUtils.getApplicationContext();

        Intent intent = mDelegate.createSettingsIntent(context, ThemeSettingsFragment.class);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.SETTINGS_URL, intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                ThemeSettingsFragment.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testCreateSettingsIntent_WithFragmentClass_UrlNavEnabled() {
        Context context = ContextUtils.getApplicationContext();

        Intent intent = mDelegate.createSettingsIntent(context, ThemeSettingsFragment.class);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals("chrome://settings/theme", intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                ThemeSettingsFragment.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    public void testFinishCurrentSettings_NullWindowAndroid_DoesNotCrash() {
        when(mMockTab.getWindowAndroid()).thenReturn(null);

        // Verify calling finishCurrentSettings when Tab is detached from window completes safely.
        mDelegate.finishCurrentSettings(mock(Fragment.class));
    }

    @Test
    public void testExecutePendingNavigations_NoHostFragment_DoesNotCrash() {
        Activity mockActivity = mock(Activity.class);

        // Verify calling executePendingNavigations with no active host fragment completes safely.
        mDelegate.executePendingNavigations(mockActivity);
    }

    @Test
    public void testRedirectFromNavigation_DefersAndReplacesCurrentEntry() {
        when(mMockTab.isInitialized()).thenReturn(true);

        mDelegate.redirectFromNavigation("chrome://settings/allSites");

        // The navigation that led here is still being committed, so nothing may happen yet.
        verify(mMockTab, never()).loadUrl(any());

        ShadowLooper.idleMainLooper();

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/allSites", captor.getValue().getUrl());
        // Replacing rather than pushing keeps the entry that could not be shown out of history.
        assertTrue(captor.getValue().getShouldReplaceCurrentEntry());
    }

    @Test
    public void testRedirectFromNavigation_UninitializedTab_DoesNothing() {
        when(mMockTab.isInitialized()).thenReturn(false);

        mDelegate.redirectFromNavigation("chrome://settings/allSites");
        ShadowLooper.idleMainLooper();

        verify(mMockTab, never()).loadUrl(any());
    }

    @Test
    public void testNavigateReplacingCurrentEntry_GoesBackWhenPreviousEntryIsSamePage() {
        setPreviousEntryUrl("chrome://settings/allSites");

        mDelegate.navigateReplacingCurrentEntry("chrome://settings/allSites");

        // Loading would leave two adjacent entries for the same page, and a back press that
        // appears to do nothing.
        verify(mMockTab).goBack();
        verify(mMockTab, never()).loadUrl(any());
    }

    @Test
    public void testNavigateReplacingCurrentEntry_LoadsWhenPreviousEntryIsADifferentPage() {
        setPreviousEntryUrl("chrome://settings/siteSettings");

        mDelegate.navigateReplacingCurrentEntry("chrome://settings/allSites");

        verify(mMockTab, never()).goBack();
        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/allSites", captor.getValue().getUrl());
        assertTrue(captor.getValue().getShouldReplaceCurrentEntry());
    }

    @Test
    // SettingsHostFragment may only exist where settings-in-tab does: both flags on, tablet width.
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
    @Config(qualifiers = "w720dp-h1024dp")
    public void testStartSettings_UnmappedFragment_WithHostFragment_ShowsFragmentInHost() {
        RecordingHostFragment hostFragment = attachHostFragment();

        Context mockContext = spy(hostFragment.requireActivity());
        doNothing().when(mockContext).startActivity(any());

        mDelegate.startSettings(mockContext, UnmappedTestFragment.class);

        // A page with no Url belongs in the host that is already open. Launching an Intent would
        // open a second settings tab at the root Url instead.
        verify(mockContext, never()).startActivity(any());
        verify(mMockTab, never()).loadUrl(any());
        assertTrue(hostFragment.mShownFragment instanceof UnmappedTestFragment);
        assertTrue(hostFragment.mShownAddToBackStack);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
    @Config(qualifiers = "w720dp-h1024dp")
    public void testStartSettings_MappedFragment_WithSearchOpen_ShowsFragmentInHost() {
        RecordingHostFragment hostFragment = attachHostFragment();
        hostFragment.setSearchOpenSupplier(() -> true);

        mDelegate.startSettings(hostFragment.requireActivity(), ThemeSettingsFragment.class);

        // The page has a Url, but it was opened from search, which owns the screen and keeps its
        // pages on the fragment back stack. Navigating the tab would purge that stack.
        verify(mMockTab, never()).loadUrl(any());
        assertTrue(hostFragment.mShownFragment instanceof ThemeSettingsFragment);
        assertTrue(hostFragment.mShownAddToBackStack);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
    @Config(qualifiers = "w720dp-h1024dp")
    public void testStartSettings_MappedFragment_WithSearchClosed_NavigatesTheTab() {
        RecordingHostFragment hostFragment = attachHostFragment();
        hostFragment.setSearchOpenSupplier(() -> false);

        mDelegate.startSettings(hostFragment.requireActivity(), ThemeSettingsFragment.class);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mMockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/theme", captor.getValue().getUrl());
        assertNull(hostFragment.mShownFragment);
    }

    /** Attaches a host fragment to a live activity and points the tab at it. */
    private RecordingHostFragment attachHostFragment() {
        TestChromeBaseAppCompatActivity activity =
                Robolectric.buildActivity(TestChromeBaseAppCompatActivity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        RecordingHostFragment hostFragment = new RecordingHostFragment();
        activity.getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        hostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mMockTab.getWindowAndroid()).thenReturn(windowAndroid);
        return hostFragment;
    }

    /**
     * Stubs out the previous navigation entry the delegate reads when deciding whether to go back.
     */
    private void setPreviousEntryUrl(String url) {
        NavigationEntry entry = mock(NavigationEntry.class);
        when(entry.getUrl()).thenReturn(new GURL(url));
        NavigationHistory history = mock(NavigationHistory.class);
        when(history.getEntryCount()).thenReturn(1);
        when(history.getEntryAtIndex(0)).thenReturn(entry);
        NavigationController controller = mock(NavigationController.class);
        when(controller.canGoBack()).thenReturn(true);
        when(controller.getDirectedNavigationHistory(false, 1)).thenReturn(history);
        WebContents webContents = mock(WebContents.class);
        when(webContents.getNavigationController()).thenReturn(controller);
        when(mMockTab.getWebContents()).thenReturn(webContents);
    }

    /** A host fragment that records what it was asked to show instead of showing it. */
    public static class RecordingHostFragment extends SettingsHostFragment {
        public Fragment mShownFragment;
        public boolean mShownAddToBackStack;

        @Override
        protected Fragment createInitialFragment(Intent intent) {
            return new UnmappedTestFragment();
        }

        @Override
        public boolean showFragment(Fragment fragment, boolean addToBackStack, String tag) {
            mShownFragment = fragment;
            mShownAddToBackStack = addToBackStack;
            return true;
        }
    }
}
