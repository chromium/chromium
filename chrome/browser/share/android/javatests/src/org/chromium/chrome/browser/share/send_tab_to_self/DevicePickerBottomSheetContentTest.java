// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DevicePickerBottomSheetContentTest {
    private static class ObservableMockWebContents extends MockWebContents {
        public WebContentsObserver observer;

        @Override
        public void addObserver(WebContentsObserver observer) {
            this.observer = observer;
        }

        @Override
        public void removeObserver(WebContentsObserver observer) {
            if (this.observer == observer) {
                this.observer = null;
            }
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;

    private WebContents mWebContents;

    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private TextFragmentReceiver.Proxy mTextFragmentReceiver;
    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private IdentityManager mIdentityManager;
    private CoreAccountInfo mCoreAccountInfo;
    private AccountInfo mAccountInfo;

    private Activity mContext;
    private PageContext mPageContext;
    private PageContext mPageContextWithScrollPosition;
    private List<TargetDeviceInfo> mDevices;

    @Before
    public void setUp() {
        mWebContents = new ObservableMockWebContents();

        IdentityServicesProvider identityServicesProvider = mock(IdentityServicesProvider.class);
        IdentityServicesProvider.setInstanceForTests(identityServicesProvider);
        when(identityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);

        mCoreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId("test@example.com", new GaiaId("test_id"));
        mAccountInfo = new AccountInfo.Builder(mCoreAccountInfo).build();

        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
        when(mIdentityManager.findExtendedAccountInfoByAccountId(any())).thenReturn(mAccountInfo);

        mContext = Robolectric.buildActivity(Activity.class).create().get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);

        mPageContext = new PageContext(new byte[] {1});
        mPageContextWithScrollPosition = new PageContext(new byte[] {2});
        mDevices = Arrays.asList(new TargetDeviceInfo("Device", "guid", FormFactor.DESKTOP, 0L));

        when(mTab.getWebContents()).thenReturn(mWebContents);
        ((MockWebContents) mWebContents).renderFrameHost = mRenderFrameHost;
        when(mRenderFrameHost.getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER))
                .thenReturn(mTextFragmentReceiver);
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com/"));

        when(mNativeMock.addScrollPositionToPageContext(any(), any()))
                .thenReturn(mPageContextWithScrollPosition);
    }

    private DevicePickerBottomSheetContent createContent() {
        return new DevicePickerBottomSheetContent(
                mContext,
                "https://example.com/",
                "Title",
                mPageContext,
                mBottomSheetController,
                mDevices,
                mProfile,
                () -> mTab);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionDisabled() {
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
        verify(mBottomSheetController).hideContent(content, true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionSuccess() {
        doAnswer(
                        invocation -> {
                            TextFragmentReceiver.RequestSelectorForViewportCenter_Response
                                    callback = invocation.getArgument(0);
                            callback.call("selector", 0, 0);
                            return null;
                        })
                .when(mTextFragmentReceiver)
                .requestSelectorForViewportCenter(any());

        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        verify(mNativeMock).addScrollPositionToPageContext(mPageContext, "selector");
        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContextWithScrollPosition));
        verify(mBottomSheetController).hideContent(content, true);

        // Verify the observer unregistered itself.
        Assert.assertNull(((ObservableMockWebContents) mWebContents).observer);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionEmptySelector() {
        doAnswer(
                        invocation -> {
                            TextFragmentReceiver.RequestSelectorForViewportCenter_Response
                                    callback = invocation.getArgument(0);
                            callback.call("", 0, 0);
                            return null;
                        })
                .when(mTextFragmentReceiver)
                .requestSelectorForViewportCenter(any());

        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        verify(mNativeMock, times(0)).addScrollPositionToPageContext(any(), any());
        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
        verify(mBottomSheetController).hideContent(content, true);

        // Verify the observer unregistered itself.
        Assert.assertNull(((ObservableMockWebContents) mWebContents).observer);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionNullTab() {
        DevicePickerBottomSheetContent content =
                new DevicePickerBottomSheetContent(
                        mContext,
                        "https://example.com/",
                        "Title",
                        mPageContext,
                        mBottomSheetController,
                        mDevices,
                        mProfile,
                        () -> null);
        content.onItemClick(null, null, 0, 0);

        verify(mBottomSheetController).hideContent(content, true);
        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionNullWebContents() {
        when(mTab.getWebContents()).thenReturn(null);
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        verify(mBottomSheetController).hideContent(content, true);
        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionUrlMismatch() {
        when(mTab.getUrl()).thenReturn(new GURL("https://different.com/"));
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        verify(mBottomSheetController).hideContent(content, true);
        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionTimeout() {
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        // Capture the observer added to the WebContents.
        WebContentsObserver observer = ((ObservableMockWebContents) mWebContents).observer;
        Assert.assertNotNull(observer);

        verify(mBottomSheetController).hideContent(content, true);
        verify(mNativeMock, times(0)).addEntry(any(), any(), any(), any(), any());

        ShadowLooper.idleMainLooper(200, java.util.concurrent.TimeUnit.MILLISECONDS);

        // Verify the observer unregistered itself.
        Assert.assertNull(((ObservableMockWebContents) mWebContents).observer);

        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionNavigationDuringCapture() {
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        // Capture the observer added to the WebContents.
        WebContentsObserver observer = ((ObservableMockWebContents) mWebContents).observer;
        Assert.assertNotNull(observer);

        // Simulate navigation.
        observer.primaryPageChanged(mock(Page.class));

        // Verify the observer unregistered itself.
        Assert.assertNull(((ObservableMockWebContents) mWebContents).observer);

        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionWebContentsDestroyedDuringCapture() {
        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        // Capture the observer added to the WebContents.
        WebContentsObserver observer = ((ObservableMockWebContents) mWebContents).observer;
        Assert.assertNotNull(observer);

        // Simulate WebContents destruction.
        observer.webContentsDestroyed();

        // Verify the observer unregistered itself.
        Assert.assertNull(((ObservableMockWebContents) mWebContents).observer);

        verify(mNativeMock)
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_SCROLL_POSITION)
    public void testSendScrollPositionCallbackIgnoredAfterNavigation() {
        // Setup the mock to capture the callback but not invoke it immediately.
        ArgumentCaptor<TextFragmentReceiver.RequestSelectorForViewportCenter_Response>
                callbackCaptor =
                        ArgumentCaptor.forClass(
                                TextFragmentReceiver.RequestSelectorForViewportCenter_Response
                                        .class);
        doAnswer(invocation -> null)
                .when(mTextFragmentReceiver)
                .requestSelectorForViewportCenter(callbackCaptor.capture());

        DevicePickerBottomSheetContent content = createContent();
        content.onItemClick(null, null, 0, 0);

        // Capture the observer added to the WebContents.
        WebContentsObserver observer = ((ObservableMockWebContents) mWebContents).observer;
        Assert.assertNotNull(observer);

        // Simulate navigation - this should trigger the fallback.
        observer.primaryPageChanged(mock(Page.class));

        // Verify fallback entry was added.
        verify(mNativeMock, times(1))
                .addEntry(
                        eq(mProfile),
                        eq("https://example.com/"),
                        eq("Title"),
                        eq("guid"),
                        eq(mPageContext));

        // Now simulate the renderer finally returning the selector (late callback).
        callbackCaptor.getValue().call("delayed-selector", 0, 0);

        // Verify that addScrollPositionToPageContext was NOT called.
        verify(mNativeMock, times(0)).addScrollPositionToPageContext(any(), any());

        // Verify that addEntry was NOT called a second time.
        verify(mNativeMock, times(1)).addEntry(any(), any(), any(), any(), any());
    }
}
