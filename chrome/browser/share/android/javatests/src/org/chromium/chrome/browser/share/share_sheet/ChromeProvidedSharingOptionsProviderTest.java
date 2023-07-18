// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;

import android.content.res.Resources;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation Unit tests {@link ChromeProvidedSharingOptionsProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.WEBNOTES_STYLIZE})
@DisableFeatures({ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO,
        ChromeFeatureList.SHARE_SHEET_CUSTOM_ACTIONS_POLISH})
@Config(shadows = ShadowGURL.class)
public class ChromeProvidedSharingOptionsProviderTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public AutomotiveContextWrapperTestRule mAutoTestRule = new AutomotiveContextWrapperTestRule();

    private static final String URL = JUnitTestGURLs.EXAMPLE_URL;

    @Mock
    private UserPrefs.Natives mUserPrefsNatives;
    @Mock
    private SendTabToSelfAndroidBridge.Natives mSendTabToSelfAndroidBridgeNatives;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private ShareSheetCoordinator mShareSheetCoordinator;
    @Mock
    private Supplier<Tab> mTabProvider;
    @Mock
    private Tab mTab;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private WebContents mWebContents;
    @Mock
    private Tracker mTracker;
    @Mock
    private ShareParams.TargetChosenCallback mTargetChosenCallback;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    private TestActivity mActivity;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        mJniMocker.mock(
                SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mSendTabToSelfAndroidBridgeNatives);
        Mockito.when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        Mockito.when(mSendTabToSelfAndroidBridgeNatives.getEntryPointDisplayReason(
                             any(), anyString()))
                .thenReturn(null);
        Mockito.when(mTabProvider.hasValue()).thenReturn(true);
        Mockito.when(mTabProvider.get()).thenReturn(mTab);
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);
        Mockito.when(mTab.getUrl()).thenReturn(new GURL(URL));
        Mockito.doNothing().when(mBottomSheetController).hideContent(any(), anyBoolean());

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @After
    public void tearDown() throws Exception {
        if (mActionTester != null) mActionTester.tearDown();
    }

    @Test
    public void getPropertyModels_longScreenshotEnabledNoTab_excludesLongScreenshot() {
        Mockito.when(mTabProvider.hasValue()).thenReturn(false);
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertFalse("Property models should not contain long screenshots.",
                propertyModelsContain(propertyModels, R.string.sharing_long_screenshot));
    }

    @Test
    public void getPropertyModels_printingEnabledNoTab_excludesPrinting() {
        Mockito.when(mTabProvider.hasValue()).thenReturn(false);
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertFalse("Property models should not contain printing.",
                propertyModelsContain(propertyModels, R.string.print_share_activity_title));
    }

    @Test
    public void getPropertyModels_printingEnabled_includesPrinting() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertTrue("Property models should contain printing.",
                propertyModelsContain(propertyModels, R.string.print_share_activity_title));
    }

    @Test
    public void getPropertyModels_multiWindow_doesNotIncludeScreenshot() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertFalse("Property models should not contain Screenshot.",
                propertyModelsContain(propertyModels, R.string.sharing_screenshot));
    }

    @Test
    public void getPropertyModels_isIncognito_doesNotIncludeQrCode() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/true,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertFalse("Property models should not contain QR Code.",
                propertyModelsContain(propertyModels, R.string.qr_code_share_icon_label));
    }

    @Test
    public void getPropertyModels_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    public void getPropertyModels_multipleTypes_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        // Long Screenshots is supported >= Android N (7.0).
        List<String> expectedModels = new ArrayList<String>();
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_screenshot));
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_long_screenshot));
        expectedModels.addAll(
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.sharing_save_image)));

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    public void getPropertyModels_doesNotFilterByDetailedContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.IMAGE), DetailedContentType.IMAGE,
                        /*isMultiWindow=*/false);

        List<String> expectedModels = new ArrayList<String>();
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_screenshot));
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_long_screenshot));
        expectedModels.addAll(
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.sharing_save_image)));

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    public void getPropertyModels_webnotes_filtersByDetailedContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.IMAGE), DetailedContentType.WEB_NOTES,
                        /*isMultiWindow=*/false);

        List<String> expectedModels =
                ImmutableList.<String>builder()
                        .add(mActivity.getResources().getString(R.string.sharing_copy_image))
                        .add(mActivity.getResources().getString(R.string.sharing_save_image))
                        .build();

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    public void getPropertyModels_onClick_callsOnTargetChosen() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.LINK);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED, /*isMultiWindow=*/false);
        View.OnClickListener onClickListener =
                propertyModels.get(0).get(ShareSheetItemViewProperties.CLICK_LISTENER);

        onClickListener.onClick(null);
        Mockito.verify(mTargetChosenCallback, Mockito.times(1))
                .onTargetChosen(ChromeProvidedSharingOptionsProvider
                                        .CHROME_PROVIDED_FEATURE_COMPONENT_NAME);
    }

    @Test
    public void getPropertyModels_onlyReturnValidOptionsForAutomotive() {
        mAutoTestRule.setIsAutomotive(true);
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE, ContentType.IMAGE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    public void getPropertyModels_linkAndTextShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_AND_TEXT,
                                ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    public void getPropertyModels_linkShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(R.string.sharing_send_tab_to_self),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    public void getPropertyModels_textShare() {
        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT), DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text)));
    }

    @Test
    public void getShareDetailsMetrics_linkGeneration() {
        @LinkGeneration
        int linkGenerationStatus = LinkGeneration.LINK;

        setUpChromeProvidedSharingOptionsProviderTest(/*isIncognito=*/false,
                /*printingEnabled=*/false, linkGenerationStatus);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT),
                        DetailedContentType.NOT_SPECIFIED, /*isMultiWindow=*/false);

        assertCorrectLinkGenerationMetrics(propertyModels, linkGenerationStatus);
    }

    private void setUpChromeProvidedSharingOptionsProviderTest(boolean isIncognito,
            boolean printingEnabled, @LinkGeneration int linkGenerationStatus) {
        Mockito.when(mPrefService.getBoolean(anyString())).thenReturn(printingEnabled);
        Mockito.when(mTab.isIncognito()).thenReturn(isIncognito);

        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"")
                                          .setCallback(mTargetChosenCallback)
                                          .setText("")
                                          .build();
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(mActivity,
                mWindowAndroid, mTabProvider, mBottomSheetController,
                new ShareSheetBottomSheetContent(mActivity, null, mShareSheetCoordinator,
                        shareParams, /*featureEngagementTracker=*/null),
                shareParams,
                /*TabPrinterDelegate=*/null, isIncognito,
                /*shareStartTime=*/0, mShareSheetCoordinator,
                /*imageEditorModuleProvider*/ null, mTracker, URL, linkGenerationStatus,
                new LinkToggleMetricsDetails(
                        LinkToggleState.COUNT, DetailedContentType.NOT_SPECIFIED),
                mProfile, mDeviceLockActivityLauncher);
    }

    private boolean propertyModelsContain(List<PropertyModel> propertyModels, int labelId) {
        for (PropertyModel propertyModel : propertyModels) {
            if (propertyModel.get(ShareSheetItemViewProperties.LABEL)
                            .equals(mActivity.getResources().getString(labelId))) {
                return true;
            }
        }
        return false;
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

    private void assertCorrectLinkGenerationMetrics(
            List<PropertyModel> propertyModels, @LinkGeneration int linkGenerationStatus) {
        mActionTester = new UserActionTester();
        View view = Mockito.mock(View.class);
        for (PropertyModel propertyModel : propertyModels) {
            String label = propertyModel.get(ShareSheetItemViewProperties.LABEL);
            Resources res = mActivity.getResources();
            // There is no link generation for Stylize Cards / Screenshots / Long Screenshots.
            if (label.equals(res.getString(R.string.sharing_webnotes_create_card))
                    || label.equals(res.getString(R.string.sharing_screenshot))
                    || label.equals(res.getString(R.string.sharing_long_screenshot))) {
                continue;
            }

            View.OnClickListener listener =
                    propertyModel.get(ShareSheetItemViewProperties.CLICK_LISTENER);
            listener.onClick(view);

            switch (linkGenerationStatus) {
                case LinkGeneration.LINK:
                    assertTrue(
                            "Expected a SharingHubAndroid...Success.LinkToTextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Success.LinkToTextShared"));
                    assertEquals("Expected a 'link' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.LINK));
                    break;
                case LinkGeneration.TEXT:
                    assertTrue("Expected a SharingHubAndroid...Success.TextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Success.TextShared"));
                    assertEquals("Expected a 'text' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.TEXT));
                    break;
                case LinkGeneration.FAILURE:
                    assertTrue("Expected a SharingHubAndroid...Failure.TextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Failure.TextShared"));
                    assertEquals("Expected a 'failure' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.FAILURE));
                    break;
                default:
                    break;
            }
        }
    }
}
