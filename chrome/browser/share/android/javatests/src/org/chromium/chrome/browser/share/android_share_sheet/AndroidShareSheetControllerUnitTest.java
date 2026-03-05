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
import android.net.Uri;
import android.os.Build;
import android.os.Parcelable;
import android.service.chooser.ChooserAction;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.StringRes;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareMetricsUtils.ShareCustomAction;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
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
import org.chromium.ui.insets.InsetObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Test for {@link AndroidShareSheetController} and {@link AndroidCustomActionProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 34)
public class AndroidShareSheetControllerUnitTest {
    private static final String SELECTOR_FOR_LINK_TO_TEXT = "selector";

    private static final Uri TEST_WEB_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.web.favicon.preview");
    private static final Uri TEST_FALLBACK_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.fallback.favicon.preview");

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    @Mock InsetObserver mInsetObserver;
    @Mock TabGroupSharingController mTabGroupSharingController;

    private TestActivity mActivity;
    private WindowAndroid mWindow;
    private PayloadCallbackHelper<Tab> mPrintCallback;
    private AndroidShareSheetController mController;
    private Bitmap mTestWebFavicon;

    @Before
    public void setup() {
        TrackerFactory.setTrackerForTests(mTracker);

        // Set up send to self option.
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mMockSendTabToSelfAndroidBridge);
        doReturn(0)
                .when(mMockSendTabToSelfAndroidBridge)
                .getEntryPointDisplayReason(any(), anyString());
        // Set up print tab option.
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        PrefService service = mock(PrefService.class);
        doReturn(service).when(mMockUserPrefsJni).get(mProfile);
        doReturn(true).when(service).getBoolean(Pref.PRINTING_ENABLED);
        // Set up favicon helper.
        FaviconHelperJni.setInstanceForTesting(mMockFaviconHelperJni);
        doReturn(1L).when(mMockFaviconHelperJni).init();
        mTestWebFavicon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(
                bitmap ->
                        mTestWebFavicon.equals(bitmap)
                                ? TEST_WEB_FAVICON_PREVIEW_URI
                                : TEST_FALLBACK_FAVICON_PREVIEW_URI);
        setFaviconToFetchForTest(mTestWebFavicon);
        // Set up mMockDomDistillerUrlUtilsJni. Needed for link-to-text sharing.
        DomDistillerUrlUtilsJni.setInstanceForTesting(mMockDomDistillerUrlUtilsJni);
        doAnswer(invocation -> new GURL(invocation.getArgument(0)))
                .when(mMockDomDistillerUrlUtilsJni)
                .getOriginalUrlFromDistillerUrl(anyString());

        doReturn(true).when(mTabGroupSharingController).isAvailableForTab(any());

        LinkToTextCoordinator.setForceSelectorForTesting(SELECTOR_FOR_LINK_TO_TEXT);

        mActivityScenario.getScenario().onActivity((activity) -> mActivity = activity);
        mActivityScenario.getScenario().moveToState(State.RESUMED);
        mWindow =
                new ActivityWindowAndroid(
                        mActivity,
                        false,
                        IntentRequestTracker.createFromActivity(mActivity),
                        mInsetObserver,
                        /* trackOcclusion= */ true);
        mPrintCallback = new PayloadCallbackHelper<>();
        // Set up mock tab
        doReturn(mWindow).when(mTab).getWindowAndroid();
        doReturn(ContextUtils.getApplicationContext()).when(mTab).getContext();

        mController =
                new AndroidShareSheetController(
                        mBottomSheetController,
                        () -> mTab,
                        () -> mTabModelSelector,
                        mProfile,
                        mPrintCallback::notifyCalled,
                        mTabGroupSharingController,
                        null);
    }

    @After
    public void tearDown() {
        mWindow.destroy();
    }

    /** Test whether custom actions are attached to the intent. */
    @Test
    @RequiresApi(api = 34)
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

        if (DeviceInfo.isDesktop()) {
            assertCustomActions(
                    intent,
                    R.string.sharing_long_screenshot,
                    R.string.sharing_send_tab_to_self,
                    R.string.qr_code_share_icon_label);
        } else {
            assertCustomActions(
                    intent,
                    R.string.sharing_tab_group,
                    R.string.sharing_long_screenshot,
                    R.string.print_share_activity_title,
                    R.string.sharing_send_tab_to_self,
                    R.string.qr_code_share_icon_label);
        }
    }

    @Test
    @RequiresApi(api = 34)
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
    public void choosePrintAction() throws CanceledException {
        Assume.assumeFalse(
                "Test ignored in the desktop mode because the Print action is not showed in the"
                        + " Share UI.",
                DeviceInfo.isDesktop());

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
                mProfile,
                mPrintCallback::notifyCalled,
                mTabGroupSharingController,
                mDeviceLockActivityLauncher);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        chooseCustomAction(intent, R.string.print_share_activity_title, ShareCustomAction.PRINT);
        Assert.assertEquals("Print callback is not called.", 1, mPrintCallback.getCallCount());
        Assert.assertEquals(
                "TargetChosenCallback is not called.", 1, callbackHelper.getCallCount());
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
    public void shareUrlWithPreviewImage() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Sharing.PreparePreviewFaviconDuration");

        Bitmap testBitmap = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(
                bitmap ->
                        testBitmap.equals(bitmap)
                                ? TEST_WEB_FAVICON_PREVIEW_URI
                                : TEST_FALLBACK_FAVICON_PREVIEW_URI);

        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setPreviewImageBitmap(testBitmap)
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
    public void shareUrlWithPreviewImageUri() {
        Bitmap testBitmap = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        // This testBitmap is unused since preview URI was set.
        Uri testUri = Uri.parse("content://test.web.favicon.preview.shareUrlWithPreviewImageUri");

        ShareParams params =
                new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setPreviewImageBitmap(testBitmap)
                        .setPreviewImageUri(testUri)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Preview clip data should not be null.", intent.getClipData());
        Assert.assertEquals(
                "Image preview Uri is null.", testUri, intent.getClipData().getItemAt(0).getUri());
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
                mProfile,
                mPrintCallback::notifyCalled,
                mTabGroupSharingController,
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
    public void shareLinkToHighlightTextFailed() {
        LinkToTextCoordinator.setForceSelectorForTesting("");

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
                mProfile,
                mPrintCallback::notifyCalled,
                mTabGroupSharingController,
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
    public void shareQrCodeForImage() throws CanceledException {
        QrCodeDialog.setInstanceForTesting(Mockito.mock(QrCodeDialog.class));
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
                QrCodeDialog.getLastUrlForTesting());
    }

    @Test
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
                "Sharing text should be the URL and Text.",
                "text" + " " + JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));

        // Attempt to do the copy image action.
        // TODO(crbug.com/40064767): Set up a real temp image and verify the URI is correct.
        chooseCustomAction(intent, R.string.sharing_copy_image, ShareCustomAction.COPY_IMAGE);
        Assert.assertTrue("Clipboard cannot paste.", Clipboard.getInstance().canPaste());
    }

    @Test
    public void webShareImageOnly() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params =
                new ShareParams.Builder(mWindow, "title", "")
                        .setText("")
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
        Assert.assertTrue(
                "Sharing text should be empty.",
                TextUtils.isEmpty(shareIntent.getStringExtra(Intent.EXTRA_TEXT)));
    }

    @Test
    public void chooseLongScreenShot() throws CanceledException {
        LongScreenshotsCoordinator mockCoordinator = Mockito.mock(LongScreenshotsCoordinator.class);
        LongScreenshotsCoordinator.setInstanceForTesting(mockCoordinator);

        ShareParams params =
                new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setBypassFixingDomDistillerUrl(true)
                        .setFileContentType("text/plain")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        if (DeviceInfo.isDesktop()) {
            assertCustomActions(
                    intent,
                    R.string.sharing_long_screenshot,
                    R.string.sharing_send_tab_to_self,
                    R.string.qr_code_share_icon_label);
        } else {
            assertCustomActions(
                    intent,
                    R.string.sharing_tab_group,
                    R.string.sharing_long_screenshot,
                    R.string.print_share_activity_title,
                    R.string.sharing_send_tab_to_self,
                    R.string.qr_code_share_icon_label);
        }
        chooseCustomAction(
                intent, R.string.sharing_long_screenshot, ShareCustomAction.LONG_SCREENSHOT);

        verify(mTracker).notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
        verify(mockCoordinator).captureScreenshot();
    }

    @Test
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

    @RequiresApi(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
            actualStringBuilder.append(",").append(((ChooserAction) action).getLabel());
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

    @RequiresApi(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private void chooseCustomAction(
            Intent chooserIntent, @StringRes int iconLabel, @ShareCustomAction int shareAction)
            throws CanceledException {
        Parcelable[] actions =
                chooserIntent.getParcelableArrayExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS);
        Assert.assertTrue("More than one action is provided.", actions.length > 0);

        // Find the print callback, since we mocked that out during this test.
        ChooserAction expectAction = null;
        for (Parcelable parcelable : actions) {
            ChooserAction chooserAction = (ChooserAction) parcelable;
            if (TextUtils.equals(
                    ContextUtils.getApplicationContext().getString(iconLabel),
                    chooserAction.getLabel())) {
                expectAction = chooserAction;
                break;
            }
        }

        Assert.assertNotNull("Print option is null when the callback is provided.", expectAction);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Sharing.SharingHubAndroid.CustomAction", shareAction)
                        .expectAnyRecord("Sharing.SharingHubAndroid.TimeToCustomAction")
                        .build();
        PendingIntent action = expectAction.getAction();
        action.send();
        RobolectricUtil.runAllBackgroundAndUi();

        histogramWatcher.assertExpected();
    }
}
