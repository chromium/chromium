// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.MoreExecutors;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.Capability;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceAvailable;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceNotAvailable;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.content_extraction.InnerTextBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ToastManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Optional;

/** Unit tests for {@link AiAssistantService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
@LooperMode(Mode.PAUSED)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY})
public class AiAssistantServiceUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private WebContents mWebContents;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private CoreAccountInfo mAccountInfo;
    @Mock SystemAiProvider mSystemAiProvider;
    @Mock SystemAiProviderFactory mSystemAiProviderFactory;
    @Mock private InnerTextBridge.Natives mInnerTextNatives;
    @Mock private AccountManagerFacade mMockFacade;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<LaunchRequest> mLaunchRequestCaptor;

    private final PausedExecutorService mPausedExecutorService = new PausedExecutorService();
    private Promise<AccountCapabilities> mCapabilitiesPromise;

    @Before
    public void setUp() throws Exception {
        ToastManager.resetForTesting();
        AiAssistantService.resetForTesting();
        when(mSystemAiProviderFactory.createSystemAiProvider()).thenReturn(mSystemAiProvider);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mTab.getProfile()).thenReturn(mProfile);
        InnerTextBridgeJni.setInstanceForTesting(mInnerTextNatives);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
    }

    @After
    public void tearDown() throws Exception {
        AiAssistantService.resetForTesting();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY + ":intent_fallback/true")
    public void showAi_fallsBackToIntentWhenNoDownstreamImpl() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(SystemAiProvider.class, null);
        var pageContents = "Page contents for one.com";
        setInnerTextExtractionResult(pageContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        var service = AiAssistantService.getInstance();

        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);
                    ShadowLooper.idleMainLooper();

                    var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
                    assertLaunchRequestIsForSummarizeUrl(
                            launchRequest, JUnitTestGURLs.URL_1.getSpec(), pageContents);
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY + ":intent_fallback/true")
    public void showAi_fallsBackToIntentWhenDownstreamImplNotAvailable() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        var pageContents = "Page contents for google.com/dog";
        setInnerTextExtractionResult(pageContents);
        setSystemAiProviderAsUnavailable();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_DOG);

        var service = AiAssistantService.getInstance();

        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);
                    ShadowLooper.idleMainLooper();

                    var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
                    assertLaunchRequestIsForSummarizeUrl(
                            launchRequest, JUnitTestGURLs.GOOGLE_URL_DOG.getSpec(), pageContents);
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY + ":intent_fallback/true")
    public void showAi_fallsBackToIntentWhenDownstreamImplNotAvailable_pdfPage() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsUnavailable();
        var pdfUri = JUnitTestGURLs.URL_1_WITH_PDF_PATH.getSpec();
        var pdfPage = mock(PdfPage.class);
        when(pdfPage.getUri()).thenReturn(Uri.parse(pdfUri));
        when(pdfPage.getTitle()).thenReturn("file.pdf");
        when(mTab.getNativePage()).thenReturn(pdfPage);

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);

                    var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
                    assertLaunchRequestIsForAnalyzeAttachment(launchRequest, pdfUri);
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY
                    + ":intent_fallback/false")
    public void showAi_showsErrorWhenDownstreamImplNotAvailableAndFallbackDisabled() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        var pageContents = "Page contents for google.com/dog";
        setInnerTextExtractionResult(pageContents);
        setSystemAiProviderAsUnavailable();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_DOG);

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);
                    ShadowLooper.idleMainLooper();
                    // Assert no intent was sent.
                    assertNull(ShadowApplication.getInstance().getNextStartedActivity());
                    // Assert system provider wasn't called.
                    verify(mSystemAiProvider, never()).launch(any(), any());
                    // Toast should be shown instead.
                    assertEquals(1, ShadowToast.shownToastCount());
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY
                    + ":intent_fallback/false")
    public void showAi_showsErrorWhenFeatureNotAvailableAndFallbackDisabled() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        var pageContents = "Page contents for google.com/dog";
        setInnerTextExtractionResult(pageContents);
        // Set system provider as available, but only for analyzing attachments.
        setSystemAiProviderAsAvailable(
                /* canUseSummarizeUrl= */ false, /* canUseAnalyzeAttachment= */ true);
        // Try to use the summarize URL feature.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_DOG);

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);
                    ShadowLooper.idleMainLooper();
                    // Assert no intent was sent.
                    assertNull(ShadowApplication.getInstance().getNextStartedActivity());
                    // Assert system provider wasn't called.
                    verify(mSystemAiProvider, never()).launch(any(), any());
                    // Toast should be shown instead.
                    assertEquals(1, ShadowToast.shownToastCount());
                });
    }

    @Test
    public void showAi_usesDownstreamImplWhenAvailable() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailableWithAllFeatures();
        var pageContents = "Page contents for URL_2";
        setInnerTextExtractionResult(pageContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);
                    service.showAi(activity, mTab);

                    ShadowLooper.idleMainLooper();

                    verify(mSystemAiProvider).launch(eq(activity), mLaunchRequestCaptor.capture());
                    assertLaunchRequestIsForSummarizeUrl(
                            mLaunchRequestCaptor.getValue(),
                            JUnitTestGURLs.URL_2.getSpec(),
                            pageContents);
                });
    }

    @Test
    public void showAi_usesAnalyzeDocumentForPdfs() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailableWithAllFeatures();
        var pdfUri = "https://google.com/file.pdf";
        var pdfPage = mock(PdfPage.class);
        when(pdfPage.getUri()).thenReturn(Uri.parse(pdfUri));
        when(pdfPage.getTitle()).thenReturn("file.pdf");
        when(mTab.getNativePage()).thenReturn(pdfPage);

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    initializeAiAssistantService(activity, service);

                    service.showAi(activity, mTab);
                    // Simulate availability query response.
                    mPausedExecutorService.runAll();

                    verify(mSystemAiProvider).launch(eq(activity), mLaunchRequestCaptor.capture());
                    assertLaunchRequestIsForAnalyzeAttachment(
                            mLaunchRequestCaptor.getValue(), pdfUri);
                });
    }

    @Test
    public void canShowAiForTab_initialCall() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setSystemAiProviderAsAvailableWithAllFeatures();

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    var result = service.canShowAiForTab(activity, mTab);

                    assertFalse(
                            "AI service should not be available, as we just queried the provider",
                            result);
                    // System provider should be queried on the first call.
                    verify(mSystemAiProvider).isAvailable(eq(activity), any());
                });
    }

    @Test
    public void canShowAiForTab_inMemoryCache() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setSystemAiProviderAsAvailableWithAllFeatures();

        var service = AiAssistantService.getInstance();
        activityScenario.onActivity(
                activity -> {
                    var firstAvailabilityResult = service.canShowAiForTab(activity, mTab);

                    assertFalse(
                            "AI service should not be available, as we just queried the provider",
                            firstAvailabilityResult);
                    // System provider should be queried on the first call.
                    verify(mSystemAiProvider).isAvailable(eq(activity), any());
                    // Simulate query response.
                    mPausedExecutorService.runAll();

                    var secondAvailabilityResult = service.canShowAiForTab(activity, mTab);

                    assertTrue(
                            "AI service should now be available, as its response was cached in"
                                    + " memory",
                            secondAvailabilityResult);

                    verifyNoMoreInteractions(mSystemAiProvider);
                });
    }

    @Test
    public void canShowAiForTab_preferenceCache() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        setSystemAiProviderAsAvailableWithAllFeatures();

        activityScenario.onActivity(
                activity -> {
                    var service = AiAssistantService.getInstance();
                    var firstAvailabilityResult = service.canShowAiForTab(activity, mTab);

                    assertFalse(
                            "AI service should not be available, as we just queried the provider",
                            firstAvailabilityResult);
                    // System provider should be queried on the first call.
                    verify(mSystemAiProvider).isAvailable(eq(activity), any());
                    // Simulate query response.
                    mPausedExecutorService.runAll();

                    service = null;
                    AiAssistantService.resetForTesting();

                    var newServiceInstance = AiAssistantService.getInstance();

                    var secondInstanceResult = newServiceInstance.canShowAiForTab(activity, mTab);

                    assertTrue(
                            "AI service should now be available, as its response was cached in"
                                    + " prefs",
                            secondInstanceResult);

                    verifyNoMoreInteractions(mSystemAiProvider);
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY
                    + ":attach_client_info/true")
    public void showAi_attachesAccountInfoWithFlag() {
        var activityScenario = mActivityScenarioRule.getScenario();
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailableWithAllFeatures();
        var pageContents = "Page contents for URL_2";
        var clientEmail = "foo@bar.com";
        setInnerTextExtractionResult(pageContents);
        setClientEmail(clientEmail);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        activityScenario.onActivity(
                activity -> {
                    var service = AiAssistantService.getInstance();
                    initializeAiAssistantService(activity, service);
                    fulfillAccountCapabilities(
                            /* isParentSupervised= */ false, /* isEnterprise= */ false);
                    service.showAi(activity, mTab);
                    ShadowLooper.idleMainLooper();

                    verify(mSystemAiProvider).launch(eq(activity), mLaunchRequestCaptor.capture());
                    assertLaunchRequestIsForSummarizeUrl(
                            mLaunchRequestCaptor.getValue(),
                            JUnitTestGURLs.URL_2.getSpec(),
                            pageContents);
                    assertLaunchRequestHasClientInfo(mLaunchRequestCaptor.getValue(), clientEmail);
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY
                    + ":attach_client_info/true")
    public void showAi_enterpriseDisables() {
        var activityScenario = mActivityScenarioRule.getScenario();

        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailableWithAllFeatures();
        var pageContents = "Page contents for URL_2";
        var clientEmail = "foo@bar.com";
        setInnerTextExtractionResult(pageContents);
        setClientEmail(clientEmail);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        activityScenario.onActivity(
                activity -> {
                    var service = AiAssistantService.getInstance();
                    initializeAiAssistantService(activity, service);
                    fulfillAccountCapabilities(
                            /* isParentSupervised= */ false, /* isEnterprise= */ true);
                    service.showAi(activity, mTab);

                    ShadowLooper.idleMainLooper();

                    verify(mSystemAiProvider, never()).launch(any(), any());
                });
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY
                    + ":attach_client_info/true")
    public void showAi_parentControlDisables() {
        var activityScenario = mActivityScenarioRule.getScenario();

        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailableWithAllFeatures();
        var pageContents = "Page contents for URL_2";
        var clientEmail = "foo@bar.com";
        setInnerTextExtractionResult(pageContents);
        setClientEmail(clientEmail);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        activityScenario.onActivity(
                activity -> {
                    var service = AiAssistantService.getInstance();
                    initializeAiAssistantService(activity, service);
                    fulfillAccountCapabilities(
                            /* isParentSupervised= */ true, /* isEnterprise= */ false);

                    service.showAi(activity, mTab);

                    ShadowLooper.idleMainLooper();

                    verify(mSystemAiProvider, never()).launch(any(), any());
                });
    }

    private void setSystemAiProviderAsAvailableWithAllFeatures() {
        setSystemAiProviderAsAvailable(
                /* canUseSummarizeUrl= */ true, /* canUseAnalyzeAttachment= */ true);
    }

    // Sets the result of calling the system provider, the result gets scheduled on
    // mPausedExecutorService, so we need to call mPausedExecutorService.runAll() afterwards to
    // simulate the query response.
    private void setSystemAiProviderAsAvailable(
            boolean canUseSummarizeUrl, boolean canUseAnalyzeAttachment) {
        var serviceAvailableBuilder = ServiceAvailable.newBuilder();

        if (canUseAnalyzeAttachment) {
            serviceAvailableBuilder.addSupportedCapabilities(
                    Capability.ANALYZE_ATTACHMENT_CAPABILITY);
        }
        if (canUseSummarizeUrl) {
            serviceAvailableBuilder.addSupportedCapabilities(Capability.SUMMARIZE_URL_CAPABILITY);
        }

        var availabilityResponse =
                AvailabilityResponse.newBuilder().setAvailable(serviceAvailableBuilder).build();
        when(mSystemAiProvider.isAvailable(any(), any()))
                .thenReturn(
                        MoreExecutors.listeningDecorator(mPausedExecutorService)
                                .submit(() -> availabilityResponse));
    }

    private void initializeAiAssistantService(Context context, AiAssistantService service) {
        service.canShowAiForTab(context, mTab);
        mPausedExecutorService.runAll();
    }

    private void fulfillAccountCapabilities(boolean isParentSupervised, boolean isEnterprise) {
        HashMap<String, Boolean> capabilities = new HashMap<>();
        capabilities.put(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                isParentSupervised);
        capabilities.put(
                AccountCapabilitiesConstants.IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME,
                isEnterprise);
        mCapabilitiesPromise.fulfill(new AccountCapabilities(capabilities));
        ShadowLooper.idleMainLooper();
    }

    private void setSystemAiProviderAsUnavailable() {
        var availabilityResponse =
                AvailabilityResponse.newBuilder()
                        .setNotAvailable(ServiceNotAvailable.getDefaultInstance())
                        .build();
        when(mSystemAiProvider.isAvailable(any(), any()))
                .thenReturn(Futures.immediateFuture(availabilityResponse));
    }

    private void setInnerTextExtractionResult(String result) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> callback =
                                    (Callback<Optional<String>>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            callback.onResult(Optional.of(result));
                            return null;
                        })
                .when(mInnerTextNatives)
                .getInnerText(eq(mRenderFrameHost), any());
    }

    private void setClientEmail(String email) {
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);
        when(mAccountInfo.getEmail()).thenReturn(email);
        AccountManagerFacadeProvider.setInstanceForTests(mMockFacade);
        mCapabilitiesPromise = new Promise<>();
        doReturn(mCapabilitiesPromise).when(mMockFacade).getAccountCapabilities(mAccountInfo);
    }

    private void assertLaunchRequestIsForSummarizeUrl(
            LaunchRequest launchRequest, String expectedUrl, String expectedPageContents) {
        assertTrue(launchRequest.hasSummarizeUrl());
        assertTrue(launchRequest.getSummarizeUrl().hasUrlContext());
        assertEquals(expectedUrl, launchRequest.getSummarizeUrl().getUrlContext().getUrl());
        assertEquals(
                expectedPageContents,
                launchRequest.getSummarizeUrl().getUrlContext().getPageContent());
    }

    private void assertLaunchRequestHasClientInfo(
            LaunchRequest launchRequest, String expectedUserEmail) {
        assertTrue(launchRequest.hasClientInfo());
        var clientInfo = launchRequest.getClientInfo();
        assertEquals(1, clientInfo.getClientAccountCount());
        var account = clientInfo.getClientAccount(0);
        assertEquals(expectedUserEmail, account.getEmail());
    }

    private void assertLaunchRequestIsForAnalyzeAttachment(
            LaunchRequest launchRequest, String expectedUri) {
        assertTrue(launchRequest.hasAnalyzeAttachment());
        assertThat(launchRequest.getAnalyzeAttachment().getFilesCount(), greaterThanOrEqualTo(1));
        assertEquals(expectedUri, launchRequest.getAnalyzeAttachment().getFiles(0).getUri());
    }

    private LaunchRequest assertVoiceActivityStartedWithLaunchRequest() {
        var startedIntent = ShadowApplication.getInstance().getNextStartedActivity();
        var launchRequestBytes =
                startedIntent.getByteArrayExtra(AiAssistantService.EXTRA_LAUNCH_REQUEST);
        assertEquals(Intent.ACTION_VOICE_COMMAND, startedIntent.getAction());

        try {
            return LaunchRequest.parseFrom(launchRequestBytes);
        } catch (InvalidProtocolBufferException ex) {
            Assert.fail("Exception while parsing proto from intent");
            return null;
        }
    }
}
