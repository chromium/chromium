// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.StringRes;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareMetricsUtils.ShareCustomAction;
import org.chromium.chrome.browser.share.android_share_sheet.AndroidShareSheetControllerUnitTest.ShadowShareImageFileUtils;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoSharingController;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoSharingControllerImpl;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Test for {@link AndroidShareSheetController} and {@link AndroidCustomActionProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowShareImageFileUtils.class, ShadowPostTask.class})
@DisableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
public class AndroidShareSheetControllerUnitTest {
    private static final String KEY_CHOOSER_ACTION_ICON = "icon";
    private static final String KEY_CHOOSER_ACTION_NAME = "name";
    private static final String KEY_CHOOSER_ACTION_ACTION = "action";
    private static final String SELECTOR_FOR_LINK_TO_TEXT = "selector";

    private static final Uri TEST_WEB_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.web.favicon.preview");
    private static final Uri TEST_FALLBACK_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.fallback.favicon.preview");

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock SendTabToSelfAndroidBridgeJni mMockSendTabToSelfAndroidBridge;
    @Mock UserPrefsJni mMockUserPrefsJni;
    @Mock FaviconHelperJni mMockFaviconHelperJni;
    @Mock DomDistillerUrlUtilsJni mMockDomDistillerUrlUtilsJni;
    @Mock BottomSheetController mBottomSheetController;
    @Mock TabModelSelector mTabModelSelector;
    @Mock Tab mTab;
    @Mock DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock Profile mProfile;
    @Mock Tracker mTracker;

    private TestActivity mActivity;
    private WindowAndroid mWindow;
    private PayloadCallbackHelper<Tab> mPrintCallback;
    private AndroidShareSheetController mController;
    private Bitmap mTestWebFavicon;

    @Before
    public void setup() {
        TrackerFactory.setTrackerForTests(mTracker);

        // Set up send to self option.
        mJniMocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mMockSendTabToSelfAndroidBridge);
        doReturn(0)
                .when(mMockSendTabToSelfAndroidBridge)
                .getEntryPointDisplayReason(any(), anyString());
        // Set up print tab option.
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        PrefService service = mock(PrefService.class);
        doReturn(service).when(mMockUserPrefsJni).get(mProfile);
        doReturn(true).when(service).getBoolean(Pref.PRINTING_ENABLED);
        // Set up favicon helper.
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mMockFaviconHelperJni);
        doReturn(1L).when(mMockFaviconHelperJni).init();
        mTestWebFavicon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        ShadowShareImageFileUtils.sExpectedWebBitmap = mTestWebFavicon;
        setFaviconToFetchForTest(mTestWebFavicon);
        // Set up mMockDomDistillerUrlUtilsJni. Needed for link-to-text sharing.
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mMockDomDistillerUrlUtilsJni);
        doAnswer(invocation -> new GURL(invocation.getArgument(0)))
                .when(mMockDomDistillerUrlUtilsJni)
                .getOriginalUrlFromDistillerUrl(anyString());
        // Setup shadow post task for clipboard actions.
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        mActivityScenario.getScenario().onActivity((activity) -> mActivity = activity);
        mActivityScenario.getScenario().moveToState(State.RESUMED);
        mWindow =
                new ActivityWindowAndroid(
                        mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        mPrintCallback = new PayloadCallbackHelper<>();
        // Set up mock tab
        doReturn(mWindow).when(mTab).getWindowAndroid();

        mController =
                new AndroidShareSheetController(
                        mBottomSheetController,
                        () -> mTab,
                        () -> mTabModelSelector,
                        () -> mProfile,
                        mPrintCallback::notifyCalled,
                        null);
    }

    @After
    public void tearDown() {
        ShadowLinkToTextCoordinator.setForceToFail(null);
        ShadowQrCodeDialog.sLastUrl = null;
        mWindow.destroy();
    }

    /** Test whether custom actions are attached to the intent. */
    @Test
    @RequiresApi(api = 34)
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void shareWithCustomAction() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setFileContentType("text/plain")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull(
                "Custom action is empty.",
                intent.getParcelableArrayExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS));

        assertCustomActions(
                intent,
                R.string.sharing_long_screenshot,
                R.string.print_share_activity_title,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);
    }

    @Test
    @RequiresApi(api = 34)
    @Config(sdk = 34)
    public void shareWithoutCustomAction() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showThirdPartyShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNull(
                "Custom action should be empty for 3p only share sheet.",
                intent.getParcelableArrayExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS));
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void choosePrintAction() throws CanceledException {
        CallbackHelper callbackHelper = new CallbackHelper();
        TargetChosenCallback callback =
                new TargetChosenCallback() {
                    @Override
                    public void onTargetChosen(ComponentName chosenComponent) {
                        callbackHelper.notifyCalled();
                    }

                    @Override
                    public void onCancel() {}
                };

        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setCallback(callback)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        AndroidShareSheetController.showShareSheet(
                params,
                chromeShareExtras,
                mBottomSheetController,
                () -> mTab,
                () -> mTabModelSelector,
                () -> mProfile,
                mPrintCallback::notifyCalled,
                mDeviceLockActivityLauncher);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        chooseCustomAction(intent, R.string.print_share_activity_title, ShareCustomAction.PRINT);
        Assert.assertEquals("Print callback is not called.", 1, mPrintCallback.getCallCount());
        Assert.assertEquals(
                "TargetChosenCallback is not called.", 1, callbackHelper.getCallCount());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void choosePageInfoAction() throws CanceledException {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();

        PageInfoSharingController mockPageInfoSharingController =
                Mockito.mock(PageInfoSharingController.class);
        PageInfoSharingControllerImpl.setInstanceForTesting(mockPageInfoSharingController);
        doReturn(true).when(mockPageInfoSharingController).shouldShowInShareSheet(mTab);
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();

        AndroidShareSheetController.showShareSheet(
                params,
                chromeShareExtras,
                mBottomSheetController,
                () -> mTab,
                () -> mTabModelSelector,
                () -> mProfile,
                mPrintCallback::notifyCalled,
                mDeviceLockActivityLauncher);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        chooseCustomAction(intent, R.string.sharing_create_summary, ShareCustomAction.PAGE_INFO);

        verify(mockPageInfoSharingController)
                .sharePageInfo(any(), eq(mBottomSheetController), any(), eq(mTab));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void chooseRemovePageInfoAction() throws CanceledException {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setText("Page info")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        // Show a share sheet containing page info.
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setIsUrlOfVisiblePage(true)
                        .setDetailedContentType(DetailedContentType.PAGE_INFO)
                        .build();

        PageInfoSharingController mockPageInfoSharingController =
                Mockito.mock(PageInfoSharingController.class);
        PageInfoSharingControllerImpl.setInstanceForTesting(mockPageInfoSharingController);
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();

        AndroidShareSheetController.showShareSheet(
                params,
                chromeShareExtras,
                mBottomSheetController,
                () -> mTab,
                () -> mTabModelSelector,
                () -> mProfile,
                mPrintCallback::notifyCalled,
                mDeviceLockActivityLauncher);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        // Share sheets with page info should have a "remove" option to share without page info.
        chooseCustomAction(
                intent, R.string.sharing_remove_summary, ShareCustomAction.REMOVE_PAGE_INFO);

        verify(mockPageInfoSharingController).shareWithoutPageInfo(any(), eq(mTab));
    }

    @Test
    public void shareImage() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileContentType("image/png")
                        .setSingleImageUri(testImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.GOOGLE_URL)
                        .setImageSrcUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent sharingIntent = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals(
                "ImageUri does not match. Page URL should be used.",
                JUnitTestGURLs.GOOGLE_URL.getSpec(),
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Test
    public void shareImageWithLinkUrl() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("image/png")
                        .setSingleImageUri(testImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.GOOGLE_URL)
                        .setImageSrcUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent sharingIntent = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals(
                "ImageUri does not match. Link URL has higher priority over page URL.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void shareImageWithCustomActions() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileContentType("image/png")
                        .setSingleImageUri(testImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.GOOGLE_URL)
                        .setImageSrcUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent,
                R.string.sharing_copy_image_with_link,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void shareImageLinkThenCopyImageAndLink() throws CanceledException {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileContentType("image/png")
                        .setSingleImageUri(testImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.GOOGLE_URL)
                        .setImageSrcUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent,
                R.string.sharing_copy_image_with_link,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);

        chooseCustomAction(
                intent,
                R.string.sharing_copy_image_with_link,
                ShareCustomAction.COPY_IMAGE_WITH_LINK);
        ClipboardManager clipboardManager =
                (ClipboardManager) mActivity.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData data = clipboardManager.getPrimaryClip();
        Assert.assertTrue(
                "Primary clip should contain image.",
                data.getDescription().filterMimeTypes("image/*").length > 0);
        Assert.assertTrue(
                "Primary clip should contain text.",
                data.getDescription().filterMimeTypes("text/*").length > 0);
        Assert.assertEquals(
                "Image being copied is different.", testImageUri, data.getItemAt(0).getUri());
        Assert.assertEquals(
                "Link being copied is different.",
                JUnitTestGURLs.GOOGLE_URL.getSpec(),
                data.getItemAt(0).getText());
    }

    @Test
    public void shareTextWithPreviewFavicon() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Sharing.PreparePreviewFaviconDuration");

        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Preview clip data should not be null.", intent.getClipData());
        Assert.assertEquals(
                "Image preview Uri is null.",
                TEST_WEB_FAVICON_PREVIEW_URI,
                intent.getClipData().getItemAt(0).getUri());
        watcher.assertExpected();
    }

    @Test
    public void shareTextWithPreviewWithFallbackFavicon() {
        // Assume favicon is not available for this test.
        setFaviconToFetchForTest(null);

        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Preview clip data should not be null.", intent.getClipData());
        Assert.assertEquals(
                "Image preview Uri is not as expected.",
                TEST_FALLBACK_FAVICON_PREVIEW_URI,
                intent.getClipData().getItemAt(0).getUri());
    }

    @Test
    public void sharePlainTextDoesNotProvidePreview() {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileContentType("text/plain")
                        .setText("text")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNull("Preview clip data should be null.", intent.getClipData());
        verifyNoMoreInteractions(mMockFaviconHelperJni);
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowLinkToTextCoordinator.class, ShadowChooserActionHelper.class})
    public void shareLinkToHighlightText() throws CanceledException {
        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setText("highlight")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        AndroidShareSheetController.showShareSheet(
                params,
                chromeShareExtras,
                mBottomSheetController,
                () -> mTab,
                () -> mTabModelSelector,
                () -> mProfile,
                mPrintCallback::notifyCalled,
                mDeviceLockActivityLauncher);

        Intent chooserIntent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent = chooserIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals(
                "Text being shared is different.",
                "\"highlight\"\n " + JUnitTestGURLs.TEXT_FRAGMENT_URL.getSpec(),
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));

        assertCustomActions(
                chooserIntent,
                R.string.sharing_copy_highlight_without_link,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);

        // Toggle the modify action again, link is removed from text.
        chooseCustomAction(
                chooserIntent,
                R.string.sharing_copy_highlight_without_link,
                ShareCustomAction.COPY_HIGHLIGHT_WITHOUT_LINK);
        ClipboardManager clipboardManager =
                (ClipboardManager) mActivity.getSystemService(Context.CLIPBOARD_SERVICE);
        Assert.assertEquals(
                "Text being copied is different.",
                "highlight",
                clipboardManager.getPrimaryClip().getItemAt(0).getText());
    }

    @Test
    @RequiresApi(34)
    @Config(
            sdk = 34,
            shadows = {ShadowLinkToTextCoordinator.class, ShadowChooserActionHelper.class})
    public void shareLinkToHighlightTextFailed() {
        ShadowLinkToTextCoordinator.setForceToFail(true);

        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setFileContentType("text/plain")
                        .setText("highlight")
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        AndroidShareSheetController.showShareSheet(
                params,
                chromeShareExtras,
                mBottomSheetController,
                () -> mTab,
                () -> mTabModelSelector,
                () -> mProfile,
                mPrintCallback::notifyCalled,
                mDeviceLockActivityLauncher);

        // Since link to share failed, the content being shared is a plain text.
        Intent chooserIntent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent = chooserIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals(
                "Text being shared is different.",
                "highlight",
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));
        Assert.assertNull(
                "Modify action should be null when generating link to text failed.",
                chooserIntent.getParcelableExtra(Intent.EXTRA_CHOOSER_MODIFY_SHARE_ACTION));
        assertCustomActions(chooserIntent);
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class, ShadowQrCodeDialog.class})
    public void shareQrCodeForImage() throws CanceledException {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "", "")
                        .setFileContentType("image/png")
                        .setSingleImageUri(testImageUri)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.GOOGLE_URL)
                        .setImageSrcUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)
                        .build();

        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent,
                R.string.sharing_copy_image_with_link,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);
        chooseCustomAction(intent, R.string.qr_code_share_icon_label, ShareCustomAction.QR_CODE);

        Assert.assertEquals(
                "Image source URL should be used for QR Code.",
                JUnitTestGURLs.GOOGLE_URL_DOGS.getSpec(),
                ShadowQrCodeDialog.sLastUrl);
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void webShareImageLink() throws CanceledException {
        Uri testImageUri = Uri.parse("content://test.image.uri/image.png");
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setText("text")
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(testImageUri)
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.WEB_SHARE)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);
        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent,
                R.string.sharing_copy_image,
                R.string.sharing_copy_image_with_link,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);

        Intent shareIntent = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals(
                "Sharing text should be the URL.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));

        // Attempt to do the copy image action.
        // TODO(crbug.com/40064767): Set up a real temp image and verify the URI is correct.
        chooseCustomAction(intent, R.string.sharing_copy_image, ShareCustomAction.COPY_IMAGE);
        Assert.assertTrue("Clipboard cannot paste.", Clipboard.getInstance().canPaste());
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void webShareImageOnly() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", "")
                        .setText("text")
                        .setBypassFixingDomDistillerUrl(true)
                        .setSingleImageUri(testImageUri)
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.WEB_SHARE)
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);
        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(intent, R.string.sharing_copy_image, R.string.sharing_send_tab_to_self);

        Intent shareIntent = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertNull(
                "Sharing text should be empty.", shareIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class, ShadowLongScreenshotsCoordinator.class})
    public void chooseLongScreenShot() throws CanceledException {
        ShadowLongScreenshotsCoordinator.sMockInstance =
                Mockito.mock(LongScreenshotsCoordinator.class);

        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setFileContentType("text/plain")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent,
                R.string.sharing_long_screenshot,
                R.string.print_share_activity_title,
                R.string.sharing_send_tab_to_self,
                R.string.qr_code_share_icon_label);
        chooseCustomAction(
                intent, R.string.sharing_long_screenshot, ShareCustomAction.LONG_SCREENSHOT);

        verify(mTracker).notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
        verify(ShadowLongScreenshotsCoordinator.sMockInstance).captureScreenshot();

        ShadowLongScreenshotsCoordinator.sMockInstance = null;
    }

    @Test
    @Config(
            sdk = 34,
            shadows = {ShadowChooserActionHelper.class})
    public void shareScreenshot() {
        Uri testImageUri = Uri.parse("content://test.screenshot.uri");
        // Build the same params and share extras as sharing a long screenshot
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", /* url= */ "")
                        .setSingleImageUri(testImageUri)
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setContentUrl(JUnitTestGURLs.EXAMPLE_URL)
                        .setDetailedContentType(ChromeShareExtras.DetailedContentType.SCREENSHOT)
                        .build();

        mController.showThirdPartyShareSheet(params, chromeShareExtras, 1L);
        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(
                intent, R.string.sharing_copy_image, R.string.sharing_copy_image_with_link);
    }

    private void setFaviconToFetchForTest(Bitmap favicon) {
        doAnswer(
                        invocation -> {
                            FaviconHelper.FaviconImageCallback callback = invocation.getArgument(4);
                            callback.onFaviconAvailable(favicon, GURL.emptyGURL());
                            return null;
                        })
                .when(mMockFaviconHelperJni)
                .getLocalFaviconImageForURL(anyLong(), eq(mProfile), any(), anyInt(), any());
    }

    private void runModifyActionFromChooserIntent(Intent chooserIntent) throws CanceledException {
        Bundle modifyAction =
                chooserIntent.getParcelableExtra(Intent.EXTRA_CHOOSER_MODIFY_SHARE_ACTION);
        PendingIntent action = modifyAction.getParcelable(KEY_CHOOSER_ACTION_ACTION);
        action.send();
        ShadowLooper.idleMainLooper();
    }

    private void assertCustomActions(Intent chooserIntent, Integer... expectedStringRes) {
        Parcelable[] actions =
                chooserIntent.getParcelableArrayExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS);
        if (expectedStringRes.length == 0) {
            Assert.assertTrue(
                    "No custom actions are expected.", actions == null || actions.length == 0);
            return;
        }

        StringBuilder actualStringBuilder = new StringBuilder();
        for (Parcelable action : actions) {
            String name = ((Bundle) action).getString(KEY_CHOOSER_ACTION_NAME);
            actualStringBuilder.append(",").append(name);
        }

        StringBuilder expectedStringBuilder = new StringBuilder();
        for (int stringRes : expectedStringRes) {
            String name = ContextUtils.getApplicationContext().getString(stringRes);
            expectedStringBuilder.append(",").append(name);
        }

        String actualString = actualStringBuilder.toString();
        String expectedString = expectedStringBuilder.toString();
        Assert.assertEquals(
                "Actions and/or the order does not match.", expectedString, actualString);
    }

    private void chooseCustomAction(
            Intent chooserIntent, @StringRes int iconLabel, @ShareCustomAction int shareAction)
            throws CanceledException {
        Parcelable[] actions =
                chooserIntent.getParcelableArrayExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS);
        Assert.assertTrue("More than one action is provided.", actions.length > 0);

        // Find the print callback, since we mocked that out during this test.
        Bundle expectAction = null;
        for (Parcelable parcelable : actions) {
            Bundle bundle = (Bundle) parcelable;
            if (TextUtils.equals(
                    ContextUtils.getApplicationContext().getResources().getString(iconLabel),
                    bundle.getString(KEY_CHOOSER_ACTION_NAME))) {
                expectAction = bundle;
                break;
            }
        }

        Assert.assertNotNull("Print option is null when the callback is provided.", expectAction);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Sharing.SharingHubAndroid.CustomAction", shareAction)
                        .expectAnyRecord("Sharing.SharingHubAndroid.TimeToCustomAction")
                        .build();
        PendingIntent action = expectAction.getParcelable(KEY_CHOOSER_ACTION_ACTION);
        action.send();
        ShadowLooper.idleMainLooper();

        histogramWatcher.assertExpected();
    }

    /** Test implementation to build a ChooserAction. */
    @Implements(ShareHelper.ChooserActionHelper.class)
    static class ShadowChooserActionHelper {
        @Implementation
        protected static boolean isSupported() {
            return true;
        }

        @Implementation
        protected static Parcelable newChooserAction(Icon icon, String name, PendingIntent action) {
            Bundle bundle = new Bundle();
            bundle.putParcelable(KEY_CHOOSER_ACTION_ICON, icon);
            bundle.putString(KEY_CHOOSER_ACTION_NAME, name);
            bundle.putParcelable(KEY_CHOOSER_ACTION_ACTION, action);
            return bundle;
        }
    }

    // Shadow class to bypass actually saving the image as URL for this test.
    @Implements(ShareImageFileUtils.class)
    static class ShadowShareImageFileUtils {
        static @Nullable Bitmap sExpectedWebBitmap;

        @Implementation
        public static void generateTemporaryUriFromBitmap(
                String fileName, Bitmap bitmap, Callback<Uri> callback) {
            if (bitmap != null && bitmap.equals(sExpectedWebBitmap)) {
                callback.onResult(TEST_WEB_FAVICON_PREVIEW_URI);
            } else {
                callback.onResult(TEST_FALLBACK_FAVICON_PREVIEW_URI);
            }
        }
    }

    /**
     * Shadow implementation of the real LinkToTextCoordinator but bypassing the selector process.
     */
    @Implements(LinkToTextCoordinator.class)
    public static class ShadowLinkToTextCoordinator {
        @RealObject private LinkToTextCoordinator mRealObj;

        public ShadowLinkToTextCoordinator() {}

        static Boolean sForceToFail;

        static void setForceToFail(Boolean forceToFail) {
            sForceToFail = forceToFail;
        }

        @Implementation
        protected void shareLinkToText() {
            boolean fail = sForceToFail != null && sForceToFail;
            mRealObj.onSelectorReady(fail ? "" : SELECTOR_FOR_LINK_TO_TEXT);
        }

        @Implementation
        protected String getTitle() {
            return "Include link: <link>";
        }
    }

    @Implements(QrCodeDialog.class)
    static class ShadowQrCodeDialog {
        static @Nullable String sLastUrl;

        @Implementation
        protected static QrCodeDialog newInstance(String url, WindowAndroid windowAndroid) {
            sLastUrl = url;
            return Mockito.mock(QrCodeDialog.class);
        }
    }

    @Implements(LongScreenshotsCoordinator.class)
    static class ShadowLongScreenshotsCoordinator {
        static LongScreenshotsCoordinator sMockInstance;

        @Implementation
        public static LongScreenshotsCoordinator create(
                Activity activity,
                Tab tab,
                String shareUrl,
                ChromeOptionShareCallback chromeOptionShareCallback,
                BottomSheetController sheetController) {
            return sMockInstance;
        }
    }
}
