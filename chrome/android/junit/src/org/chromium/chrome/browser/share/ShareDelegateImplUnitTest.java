// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareContentType;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareSheetDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowAndroidShareSheetController;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowShareHelper;
import org.chromium.chrome.browser.share.ShareDelegateImplUnitTest.ShadowShareSheetCoordinator;
import org.chromium.chrome.browser.share.android_share_sheet.AndroidShareSheetController;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link ShareDelegateImpl} that mocked out most native class calls. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            ShadowShareSheetCoordinator.class,
            ShadowShareHelper.class,
            ShadowAndroidShareSheetController.class,
        })
public class ShareDelegateImplUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private Tracker mTracker;

    private ShareDelegateImpl mShareDelegate;

    private void createShareDelegate(boolean isCustomTab) {
        mShareDelegate =
                new ShareDelegateImpl(
                        mBottomSheetController,
                        mActivityLifecycleDispatcher,
                        () -> mTab,
                        () -> mTabModelSelector,
                        () -> mProfile,
                        new ShareSheetDelegate(),
                        isCustomTab);
    }

    @Before
    public void setup() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);
        TrackerFactory.setTrackerForTests(mTracker);
        Mockito.doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        createShareDelegate(false);
    }

    @After
    public void tearDown() {
        ShadowShareSheetCoordinator.reset();
        ShadowShareHelper.reset();
        ShadowAndroidShareSheetController.reset();
    }

    @Test
    public void shareWithSharingHub() {
        Assert.assertTrue("ShareHub not enabled.", mShareDelegate.isSharingHubEnabled());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Sharing.SharingHubAndroid.ShareContentType")
                        .expectAnyRecord("Sharing.SharingHubAndroid.Opened")
                        .build();
        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertTrue(
                "ShareSheetCoordinator not used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
        histogramWatcher.assertExpected();
    }

    @Test
    public void shareLastUsedComponent() {
        Assert.assertTrue("ShareHub not enabled.", mShareDelegate.isSharingHubEnabled());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sharing.SharingHubAndroid.ShareContentType")
                        .expectNoRecords("Sharing.SharingHubAndroid.Opened")
                        .build();
        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setShareDirectly(true).build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertFalse(
                "ShareSheetCoordinator should not be used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
        Assert.assertTrue(
                "ShareWithLastUsedComponentCalled not called.",
                ShadowShareHelper.sShareWithLastUsedComponentCalled);
        histogramWatcher.assertExpected();
    }

    @Test
    @Config(sdk = 34)
    public void shareWithAndroidShareSheetForU() {
        Assert.assertFalse("ShareHub enabled.", mShareDelegate.isSharingHubEnabled());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Sharing.DefaultSharesheetAndroid.ShareContentType")
                        .expectAnyRecord("Sharing.DefaultSharesheetAndroid.Opened")
                        .build();

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertFalse(
                "ShareSheetCoordinator should not be used.",
                ShadowShareSheetCoordinator.sChromeShareSheetShowed);
        Assert.assertTrue(
                "shareWithSystemShareSheetUi not called.",
                ShadowAndroidShareSheetController.sShareWithSystemShareSheetUiCalled);
        histogramWatcher.assertExpected();
    }

    @Test
    public void androidShareSheetDisableNonU() {
        Assert.assertTrue("ShareHub should be enabled T-.", mShareDelegate.isSharingHubEnabled());
    }

    @Test
    public void testGetShareContentType_link() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.LINK.",
                ShareContentType.LINK,
                ShareDelegateImpl.getShareContentType(params, extras));

        params =
                new ShareParams.Builder(
                                mWindowAndroid, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setPreviewImageUri(Uri.parse("content://path/to/preview"))
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Title and preview does not impact types. Expected ShareContentType.LINK.",
                ShareContentType.LINK,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_linkWithText() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setText("text")
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.TEXT_WITH_LINK.",
                ShareContentType.TEXT_WITH_LINK,
                ShareDelegateImpl.getShareContentType(params, extras));

        params =
                new ShareParams.Builder(
                                mWindowAndroid, "", JUnitTestGURLs.TEXT_FRAGMENT_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setText("text")
                        .setLinkToTextSuccessful(true)
                        .build();
        extras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        Assert.assertEquals(
                "Expected ShareContentType.TEXT_WITH_LINK.",
                ShareContentType.TEXT_WITH_LINK,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_Image() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(Uri.parse("content://path/to/image1"))
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.IMAGE.",
                ShareContentType.IMAGE,
                ShareDelegateImpl.getShareContentType(params, extras));

        // Multiple image should be the same.
        params =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setBypassFixingDomDistillerUrl(true)
                        .setFileUris(
                                new ArrayList<>(
                                        List.of(
                                                Uri.parse("content://path/to/image1"),
                                                Uri.parse("content://path/to/image2"))))
                        .setLinkToTextSuccessful(true)
                        .setFileContentType("image/png")
                        .build();
        extras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        Assert.assertEquals(
                "Expected ShareContentType.IMAGE.",
                ShareContentType.IMAGE,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_imageWithLink() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(Uri.parse("content://path/to/image1"))
                        .setFileContentType("image/png")
                        .setText("text") // text is ignored.
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.IMAGE_WITH_LINK.",
                ShareContentType.IMAGE_WITH_LINK,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_files() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(Uri.parse("content://path/to/video1"))
                        .setFileContentType("video/mp4")
                        .setText("text") // text is ignored.
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.FILES.",
                ShareContentType.FILES,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_text() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setBypassFixingDomDistillerUrl(true)
                        .setPreviewImageUri(
                                Uri.parse("content://path/to/preview")) // preview is ignored.
                        .setText("text")
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.TEXT.",
                ShareContentType.TEXT,
                ShareDelegateImpl.getShareContentType(params, extras));
    }

    @Test
    public void testGetShareContentType_unknown() {
        ShareParams params =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras extras = new ChromeShareExtras.Builder().build();
        Assert.assertEquals(
                "Expected ShareContentType.UNKNOWN.",
                ShareContentType.UNKNOWN,
                ShareDelegateImpl.getShareContentType(params, extras));
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
        protected void __constructor__(
                BottomSheetController controller,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                Supplier<Tab> tabProvider,
                Callback<Tab> printTab,
                LargeIconBridge iconBridge,
                boolean isIncognito,
                Tracker featureEngagementTracker,
                Profile profile) {
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
        public static void showShareSheet(
                ShareParams params,
                ChromeShareExtras chromeShareExtras,
                BottomSheetController controller,
                Supplier<Tab> tabProvider,
                Supplier<TabModelSelector> tabModelSelectorSupplier,
                Supplier<Profile> profileSupplier,
                Callback<Tab> printCallback,
                DeviceLockActivityLauncher deviceLockActivityLauncher) {
            sShareWithSystemShareSheetUiCalled = true;
        }

        public static void reset() {
            sShareWithSystemShareSheetUiCalled = false;
        }
    }
}
