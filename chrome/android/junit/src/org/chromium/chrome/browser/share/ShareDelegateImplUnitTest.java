// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.AppHooksImpl;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareSheetDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowAndroidShareSheetController;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowShareHelper;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowShareSheetCoordinator;
import org.chromium.chrome.browser.share.android_share_sheet.AndroidShareSheetController;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Unit test for {@link ShareDelegateImpl} that mocked out most native class calls.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowShareSheetCoordinator.class, ShadowShareHelper.class,
                ShadowAndroidShareSheetController.class})
@Features.EnableFeatures(ChromeFeatureList.SHARE_SHEET_MIGRATION_ANDROID)
public class ShareDelegateImplUnitTest {
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private Profile mProfile;
    @Mock
    private Tab mTab;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private Activity mActivity;
    @Mock
    private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock
    private AppHooksImpl mAppHooks;
    @Mock
    private Tracker mTracker;

    private ShareDelegateImpl mShareDelegate;

    @Before
    public void setup() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);
        AppHooks.setInstanceForTesting(mAppHooks);
        TrackerFactory.setTrackerForTests(mTracker);
        Mockito.doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();

        mShareDelegate = new ShareDelegateImpl(mBottomSheetController, mActivityLifecycleDispatcher,
                (() -> mTab), (() -> mTabModelSelector), (() -> mProfile), new ShareSheetDelegate(),
                false);
    }

    @After
    public void tearDown() {
        AppHooks.setInstanceForTesting(null);
        TrackerFactory.setTrackerForTests(null);
        ShadowShareSheetCoordinator.reset();
        ShadowShareHelper.reset();
        ShadowAndroidShareSheetController.reset();
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SHARE_SHEET_MIGRATION_ANDROID)
    public void shareWithSharingHub() {
        Assert.assertTrue("ShareHub not enabled.", mShareDelegate.isSharingHubEnabled());

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertTrue("ShareSheetCoordinator not used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SHARE_SHEET_MIGRATION_ANDROID)
    public void shareLastUsedComponent() {
        Assert.assertTrue("ShareHub not enabled.", mShareDelegate.isSharingHubEnabled());

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setShareDirectly(true).build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertFalse("ShareSheetCoordinator should not be used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
        Assert.assertTrue("ShareWithLastUsedComponentCalled not called.",
                ShadowShareHelper.sShareWithLastUsedComponentCalled);
    }

    @Test
    public void shareWithAndroidShareSheet() {
        Assert.assertFalse("ShareHub enabled.", mShareDelegate.isSharingHubEnabled());

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertFalse("ShareSheetCoordinator should not be used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
        Assert.assertTrue("shareWithSystemShareSheetUi not called.",
                ShadowAndroidShareSheetController.sShareWithSystemShareSheetUiCalled);
    }

    @Implements(ShareHelper.class)
    static class ShadowShareHelper {
        static boolean sShareWithLastUsedComponentCalled;

        @Implementation
        protected static void shareWithLastUsedComponent(@NonNull ShareParams params) {
            sShareWithLastUsedComponentCalled = true;
        }

        public static void reset() {
            sShareWithLastUsedComponentCalled = false;
        }
    }

    /** Convenient class to avoid creating the real ShareSheetDelegate. */
    @Implements(ShareSheetCoordinator.class)
    public static class ShadowShareSheetCoordinator {
        static boolean sChromeShareSheetShowed;

        public ShadowShareSheetCoordinator() {}

        @Implementation
        protected void __constructor__(BottomSheetController controller,
                ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
                Callback<Tab> printTab, LargeIconBridge iconBridge, boolean isIncognito,
                ImageEditorModuleProvider imageEditorModuleProvider,
                Tracker featureEngagementTracker, Profile profile) {
            // Leave blank to avoid creating unnecessary objects.
        }

        @Implementation
        protected void showInitialShareSheet(
                ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
            sChromeShareSheetShowed = true;
        }

        public static void reset() {
            sChromeShareSheetShowed = false;
        }
    }

    @Implements(AndroidShareSheetController.class)
    static class ShadowAndroidShareSheetController {
        static boolean sShareWithSystemShareSheetUiCalled;

        // Directly call share helper, as we don't care about whether the right params are used in
        // this test.
        @Implementation
        public static void showShareSheet(ShareParams params, ChromeShareExtras chromeShareExtras,
                BottomSheetController controller, Supplier<Tab> tabProvider,
                Supplier<TabModelSelector> tabModelSelectorSupplier,
                Supplier<Profile> profileSupplier, Callback<Tab> printCallback) {
            sShareWithSystemShareSheetUiCalled = true;
        }

        public static void reset() {
            sShareWithSystemShareSheetUiCalled = false;
        }
    }
}
