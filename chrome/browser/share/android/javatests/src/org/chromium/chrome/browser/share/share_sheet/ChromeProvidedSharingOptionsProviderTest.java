// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyString;

import android.app.Activity;
import android.support.test.rule.ActivityTestRule;

import androidx.test.filters.MediumTest;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Tests {@link ChromeProvidedSharingOptionsProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeProvidedSharingOptionsProviderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private UserPrefs.Natives mUserPrefsNatives;

    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;

    private static final String URL = "http://www.google.com/";

    @Mock
    private ShareSheetCoordinator mShareSheetCoordinator;

    private Activity mActivity;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;

    @Mock
    private Supplier<Tab> mTabProvider;

    @Mock
    private Tab mTab;

    @Mock
    private WebContents mWebContents;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        Mockito.when(mTabProvider.get()).thenReturn(mTab);
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);
        Mockito.when(mTab.getUrl()).thenReturn(new GURL(URL));
        Mockito.when(mWebContents.isIncognito()).thenReturn(false);
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_screenshotQrCodeEnabled_includesBoth() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES, /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
            ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_screenshotQrCodeDisabled_doesNotIncludeEither() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES, /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(
                        R.string.send_tab_to_self_share_activity_title)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
            ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_printingEnabled_includesPrinting() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES, /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(
                                         R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.print_share_activity_title)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE,
                    ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    getPropertyModels_sharingHub15Enabled_includesCopyText() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT), /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
                    ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    getPropertyModels_linkAndTextShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_AND_TEXT,
                                ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT),
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
                    ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    getPropertyModels_linkShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.DisableFeatures(
            {ChromeFeatureList.CHROME_SHARE_QRCODE, ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
                    ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    getPropertyModels_textShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT),
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_QRCODE,
            ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID,
            ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_multiWindow_doesNotIncludeScreenshot() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES, /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(
                        R.string.send_tab_to_self_share_activity_title)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            {ChromeFeatureList.CHROME_SHARE_SCREENSHOT, ChromeFeatureList.CHROME_SHARE_QRCODE})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getPropertyModels_multipleTypes_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/true);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE),
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void getPropertyModels_sharingHub15Disabled_noHighlights() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT), /*isMultiWindow=*/false);

        assertEquals("Incorrect number of property models.", 0, propertyModels.size());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15,
            ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID})
    public void
    getPropertyModels_sharingHub15HighlightsEnabled() {
        setUpChromeProvidedSharingOptionsProviderTest(/*printingEnabled=*/false);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT), /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text),
                        mActivity.getResources().getString(R.string.sharing_highlights)));
    }

    @Test
    @MediumTest
    public void getUrlToShare_noShareParamsUrl_returnsImageUrl() {
        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setImageSrcUrl(URL).build();

        assertEquals("URL should be imageSrcUrl.",
                ChromeProvidedSharingOptionsProvider.getUrlToShare(
                        shareParams, chromeShareExtras, ""),
                URL);
    }

    @Test
    @MediumTest
    public void getUrlToShare_shareParamsUrlExists_returnsShareParamsUrl() {
        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", URL).build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setImageSrcUrl("").build();

        assertEquals("URL should be ShareParams URL.",
                ChromeProvidedSharingOptionsProvider.getUrlToShare(
                        shareParams, chromeShareExtras, ""),
                URL);
    }

    @Test
    @MediumTest
    public void getUrlToShare_noShareParamsUrl_noImageUrl() {
        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setImageSrcUrl("").build();

        assertEquals("URL should be ShareParams URL.",
                ChromeProvidedSharingOptionsProvider.getUrlToShare(
                        shareParams, chromeShareExtras, URL),
                URL);
    }

    private void setUpChromeProvidedSharingOptionsProviderTest(boolean printingEnabled) {
        Mockito.when(mPrefService.getBoolean(anyString())).thenReturn(printingEnabled);

        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"").build();
        mChromeProvidedSharingOptionsProvider =
                new ChromeProvidedSharingOptionsProvider(mActivity, mTabProvider,
                        /*bottomSheetController=*/null,
                        new ShareSheetBottomSheetContent(
                                mActivity, null, mShareSheetCoordinator, shareParams),
                        new ShareParams.Builder(null, "", "").build(),
                        new ChromeShareExtras.Builder().build(),
                        /*TabPrinterDelegate=*/null,
                        /*settingsLauncher=*/null,
                        /*syncState=*/false,
                        /*shareStartTime=*/0, mShareSheetCoordinator);
    }

    private void assertCorrectModelsAreInTheRightOrder(
            List<PropertyModel> propertyModels, List<String> expectedOrder) {
        ImmutableList.Builder<String> actualLabelOrder = ImmutableList.builder();
        for (PropertyModel propertyModel : propertyModels) {
            actualLabelOrder.add(propertyModel.get(ShareSheetItemViewProperties.LABEL));
        }
        assertEquals(
                "Property models in the wrong order.", expectedOrder, actualLabelOrder.build());
    }
}
