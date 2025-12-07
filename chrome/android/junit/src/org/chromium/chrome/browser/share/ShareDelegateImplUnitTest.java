// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridge;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridgeJni;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareContentType;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareSheetDelegate;
import org.chromium.chrome.browser.share.android_share_sheet.AndroidShareSheetController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link ShareDelegateImpl} that mocked out most native class calls. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShareDelegateImplUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Mock private Context mContext;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ShareSheetDelegate mShareSheetController;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private Tracker mTracker;
    @Mock private DataSharingTabManager mDataSharingTabManager;

    @Mock private DataProtectionBridge.Natives mDataProtectionBridgeMock;

    private final ArgumentCaptor<ShareParams> mShareParamsCaptor =
            ArgumentCaptor.forClass(ShareParams.class);

    private ShareDelegateImpl mShareDelegate;

    private static final Answer<Object> sShareIsAllowedByPolicy =
            (invocation) -> {
                Callback<Boolean> callback = invocation.getArgument(2);
                callback.onResult(true);
                return null;
            };
    private static final Answer<Object> sShareIsNotAllowedByPolicy =
            (invocation) -> {
                Callback<Boolean> callback = invocation.getArgument(2);
                callback.onResult(false);
                return null;
            };
    private int mAndroidShareSheetCallCount;
    private int mDelegateShareSheetHubDisabledCallCount;
    private int mDelegateShareSheetHubEnabledCallCount;
    private int mShareHelperCallCount;

    private void createShareDelegate(boolean isCustomTab, ShareSheetDelegate shareSheetDelegate) {
        mShareDelegate =
                new ShareDelegateImpl(
                        mContext,
                        mBottomSheetController,
                        mActivityLifecycleDispatcher,
                        () -> mTab,
                        () -> mTabModelSelector,
                        () -> mProfile,
                        shareSheetDelegate,
                        isCustomTab,
                        mDataSharingTabManager);
    }

    @Before
    public void setup() {
        AndroidShareSheetController.setShowShareSheetHookForTesting(
                () -> mAndroidShareSheetCallCount++);
        ShareDelegateImpl.setShowShareSheetHookForTesting(
                sharingHubEnabled -> {
                    if (sharingHubEnabled) {
                        mDelegateShareSheetHubEnabledCallCount++;
                    } else {
                        mDelegateShareSheetHubDisabledCallCount++;
                    }
                });
        ShareHelper.setShareWithLastUsedComponentHookForTesting(() -> mShareHelperCallCount++);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        TrackerFactory.setTrackerForTests(mTracker);
        Mockito.doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        DataProtectionBridgeJni.setInstanceForTesting(mDataProtectionBridgeMock);

        // TODO(crbug.com/406591712): Update to stubbing share methods when those are added.
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyTextIsAllowedByPolicy(anyString(), any(), any());
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyUrlIsAllowedByPolicy(anyString(), any(), any());
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyImageIsAllowedByPolicy(anyString(), any(), any());

        createShareDelegate(false, new ShareSheetDelegate());
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

        Assert.assertEquals(0, mDelegateShareSheetHubDisabledCallCount);
        Assert.assertEquals(1, mDelegateShareSheetHubEnabledCallCount);
        Assert.assertEquals(0, mAndroidShareSheetCallCount);
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

        Assert.assertEquals(0, mDelegateShareSheetHubDisabledCallCount);
        Assert.assertEquals(0, mDelegateShareSheetHubEnabledCallCount);
        Assert.assertEquals(1, mShareHelperCallCount);
        histogramWatcher.assertExpected();
    }

    @Test
    @Config(sdk = 34)
    public void shareWithAndroidShareSheetForU() {
        // Set CaRMA phase 2 compliance, which guarantees the Android share sheet on automotive
        // devices.
        AutomotiveUtils.setCarmaPhase2ComplianceForTesting(true);

        Assert.assertFalse("ShareHub enabled.", mShareDelegate.isSharingHubEnabled());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Sharing.DefaultSharesheetAndroid.ShareContentType")
                        .expectAnyRecord("Sharing.DefaultSharesheetAndroid.Opened")
                        .build();

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.OVERFLOW_MENU);

        Assert.assertEquals(1, mDelegateShareSheetHubDisabledCallCount);
        Assert.assertEquals(0, mDelegateShareSheetHubEnabledCallCount);
        Assert.assertEquals(0, mAndroidShareSheetCallCount);
        histogramWatcher.assertExpected();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareText_allowedByPolicy() {
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareTextIsAllowedByPolicy(anyString(), any(), any());
        String shareText = "shareText";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setText(shareText).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
        Assert.assertEquals(shareText, mShareParamsCaptor.getValue().getText());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareText_notAllowedByPolicy() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareTextIsAllowedByPolicy(anyString(), any(), any());
        String shareText = "shareText";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setText(shareText).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectNotAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareText_emptyText_bypassesPolicyCheck() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareTextIsAllowedByPolicy(anyString(), any(), any());

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setText("").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareLink_allowedByPolicy() {
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareUrlIsAllowedByPolicy(anyString(), any(), any());
        String shareUrl = "share_url.com";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", shareUrl)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
        Assert.assertEquals(shareUrl, mShareParamsCaptor.getValue().getUrl());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareLink_notAllowedByPolicy() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareUrlIsAllowedByPolicy(anyString(), any(), any());
        String shareUrl = "share_url.com";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", shareUrl)
                        .setBypassFixingDomDistillerUrl(true)
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectNotAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareLink_emptyUrl_bypassesPolicyCheck() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareUrlIsAllowedByPolicy(anyString(), any(), any());

        ShareParams shareParams = new ShareParams.Builder(mWindowAndroid, "", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareImage_allowedByPolicy() {
        doAnswer(sShareIsAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareImageIsAllowedByPolicy(anyString(), any(), any());
        Uri imageUri = Mockito.mock(Uri.class);
        doReturn("imageUriPath").when(imageUri).getPath();

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setSingleImageUri(imageUri)
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
        Assert.assertEquals(imageUri, mShareParamsCaptor.getValue().getSingleImageUri());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareImage_notAllowedByPolicy() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyShareImageIsAllowedByPolicy(anyString(), any(), any());
        Uri imageUri = Mockito.mock(Uri.class);
        doReturn("imageUriPath").when(imageUri).getPath();

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "")
                        .setSingleImageUri(imageUri)
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectNotAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShareImage_emptyUrl_bypassesPolicyCheck() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyImageIsAllowedByPolicy(anyString(), any(), any());

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setSingleImageUri(null).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShare_nullRenderFrameHost_bypassesPolicyCheck() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyTextIsAllowedByPolicy(anyString(), any(), any());
        String shareText = "shareText";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setText(shareText).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(null).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
        Assert.assertEquals(shareText, mShareParamsCaptor.getValue().getText());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)
    public void testShare_featureFlagDisabled_bypassesPolicyCheck() {
        doAnswer(sShareIsNotAllowedByPolicy)
                .when(mDataProtectionBridgeMock)
                .verifyCopyTextIsAllowedByPolicy(anyString(), any(), any());
        String shareText = "shareText";

        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, "", "").setText(shareText).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setRenderFrameHost(mRenderFrameHost).build();

        testShareExpectAllowed(shareParams, chromeShareExtras);
        Assert.assertEquals(shareText, mShareParamsCaptor.getValue().getText());
    }

    private void testShareExpectAllowed(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        createShareDelegate(false, mShareSheetController);
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.CONTEXT_MENU);
        verify(mShareSheetController)
                .share(
                        mShareParamsCaptor.capture(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyLong(),
                        anyBoolean());
    }

    private void testShareExpectNotAllowed(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        createShareDelegate(false, mShareSheetController);
        mShareDelegate.share(shareParams, chromeShareExtras, ShareOrigin.CONTEXT_MENU);
        verify(mShareSheetController, never())
                .share(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyLong(),
                        anyBoolean());
    }

    @Test
    public void androidShareSheetDisableNonU() {
        Assert.assertTrue("ShareHub should be enabled T-.", mShareDelegate.isSharingHubEnabled());
    }

    @Test
    @Config(sdk = 35)
    public void share_automotiveV_useAndroidShareSheet() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.setCarmaPhase2ComplianceForTesting(false);
        Assert.assertFalse(
                "Automotive devices should be using the OS share sheet on V+.",
                mShareDelegate.isSharingHubEnabled());
    }

    @Test
    public void share_autoU_noCarmaCompliance_useCustomShareSheet() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.setCarmaPhase2ComplianceForTesting(false);
        Assert.assertTrue(
                "Custom share sheet should still be used on U- auto devices without CaRMA"
                        + " compliance.",
                mShareDelegate.isSharingHubEnabled());
    }

    @Test
    @Config(sdk = 34)
    public void share_auto_withCarmaCompliance_useOsShareSheet() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.setCarmaPhase2ComplianceForTesting(true);
        Assert.assertFalse(
                "Auto devices with CaRMA Phase 2 compliance support the OS share sheet.",
                mShareDelegate.isSharingHubEnabled());
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

    @Test
    public void testSharePDf() {
        final String pdfTitle = "menu.pdf";
        final String contentUri = "content://media/external/downloads/1000000022";
        final String pdfUrl = PdfUtils.encodePdfPageUrl(contentUri);
        doReturn(true).when(mTab).isNativePage();
        doReturn(new GURL(pdfUrl)).when(mTab).getUrl();
        doReturn(pdfTitle).when(mTab).getTitle();
        doReturn(mock(WindowAndroid.class)).when(mTab).getWindowAndroid();

        createShareDelegate(false, mShareSheetController);
        mShareDelegate.share(mTab, false, ShareOrigin.OVERFLOW_MENU);
        verify(mShareSheetController)
                .share(
                        mShareParamsCaptor.capture(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyLong(),
                        anyBoolean());

        ShareParams params = mShareParamsCaptor.getValue();
        Assert.assertEquals(
                "Incorrect file URI size on ShareParams.", 1, params.getFileUris().size());
        Assert.assertEquals(
                "PDF file content URI should be set on ShareParams.",
                contentUri,
                params.getFileUris().get(0).toString());
        Assert.assertEquals(
                "Page title should be set on ShareParams.", pdfTitle, params.getTitle());
        Assert.assertEquals("URL should be empty on ShareParams.", "", params.getUrl());
    }
}
