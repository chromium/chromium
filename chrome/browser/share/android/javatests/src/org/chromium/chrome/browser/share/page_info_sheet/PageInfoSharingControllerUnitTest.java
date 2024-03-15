// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
public class PageInfoSharingControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private PageInfoSharingBridgeJni mPageInfoSharingBridgeJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        PageInfoSharingControllerImpl.resetForTesting();
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
        mJniMocker.mock(PageInfoSharingBridgeJni.TEST_HOOKS, mPageInfoSharingBridgeJni);
        when(mDomDistillerUrlUtilsJni.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenAnswer(
                        (invocation) -> {
                            return new GURL((String) invocation.getArguments()[0]);
                        });
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    public void testIsAvailable_withFeatureDisabled() {
        Tab tab = Mockito.mock(Tab.class);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNullTab() {
        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(null));
    }

    @Test
    public void testIsAvailable_withNullUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(null);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNonHttpUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_ABOUT);
        when(tab.getProfile()).thenReturn(mProfile);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withHttpUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(tab.getProfile()).thenReturn(mProfile);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(eq(mProfile))).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(eq(tab))).thenReturn(true);

        assertTrue(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withUnsupportedProfile() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(tab.getProfile()).thenReturn(mProfile);

        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(false);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withUnsupportedLanguage() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(tab.getProfile()).thenReturn(mProfile);

        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(false);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_whileSharingAnotherTab() {
        Tab firstTab = Mockito.mock(Tab.class);
        when(firstTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(firstTab.getProfile()).thenReturn(mProfile);
        Tab secondTab = Mockito.mock(Tab.class);
        when(secondTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);
        when(secondTab.getProfile()).thenReturn(mProfile);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(Mockito.any())).thenReturn(true);

        ChromeOptionShareCallback optionShareCallback = mock(ChromeOptionShareCallback.class);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            optionShareCallback,
                                            firstTab);
                            assertFalse(
                                    "Page sharing process should only happen for one tab at a time",
                                    PageInfoSharingControllerImpl.getInstance()
                                            .isAvailableForTab(secondTab));
                        });
    }

    @Test
    public void testSharePageInfo_ensureSheetOpens() {
        ChromeOptionShareCallback optionShareCallback = mock(ChromeOptionShareCallback.class);
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(tab.getProfile()).thenReturn(mProfile);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            optionShareCallback,
                                            tab);
                            verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
                        });
    }

    @Test
    public void testSharePageInfo_ensureSheetMovesBackToShare() {
        ChromeOptionShareCallback optionShareCallback = mock(ChromeOptionShareCallback.class);
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(tab.getProfile()).thenReturn(mProfile);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        when(tab.getTitle()).thenReturn("Page title");

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            optionShareCallback,
                                            tab);
                            ArgumentCaptor<BottomSheetContent> bottomSheetContentArgumentCaptor =
                                    ArgumentCaptor.forClass(BottomSheetContent.class);
                            verify(mBottomSheetController)
                                    .requestShowContent(
                                            bottomSheetContentArgumentCaptor.capture(),
                                            anyBoolean());

                            android.view.View acceptButton =
                                    bottomSheetContentArgumentCaptor
                                            .getValue()
                                            .getContentView()
                                            .findViewById(R.id.accept_button);
                            // Click accept button on shown bottom sheet.
                            assertTrue(acceptButton.isEnabled());
                            acceptButton.performClick();

                            ArgumentCaptor<ChromeShareExtras> chromeShareExtrasArgumentCaptor =
                                    ArgumentCaptor.forClass(ChromeShareExtras.class);
                            ArgumentCaptor<ShareParams> shareParamsArgumentCaptor =
                                    ArgumentCaptor.forClass(ShareParams.class);
                            // New share sheet should be opened.
                            verify(optionShareCallback)
                                    .showShareSheet(
                                            shareParamsArgumentCaptor.capture(),
                                            chromeShareExtrasArgumentCaptor.capture(),
                                            anyLong());

                            // Share params should include page info content type.
                            assertEquals(
                                    DetailedContentType.PAGE_INFO,
                                    chromeShareExtrasArgumentCaptor
                                            .getValue()
                                            .getDetailedContentType());
                            assertEquals(
                                    "Page title", shareParamsArgumentCaptor.getValue().getText());
                        });
    }
}
