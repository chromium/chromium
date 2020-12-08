// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import androidx.test.filters.MediumTest;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Tests {@link ShareSheetPropertyModelBuilder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB})
public final class ShareSheetPropertyModelBuilderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private PackageManager mPackageManager;
    @Mock
    private ShareParams mParams;
    @Mock
    private ResolveInfo mTextResolveInfo1;
    @Mock
    private ResolveInfo mTextResolveInfo2;
    @Mock
    private ResolveInfo mTextResolveInfo3;
    @Mock
    private ResolveInfo mImageResolveInfo1;
    @Mock
    private ResolveInfo mImageResolveInfo2;

    private static final String sTextModelLabel1 = "textModelLabel1";
    private static final String sTextModelLabel2 = "textModelLabel2";
    private static final String sImageModelLabel1 = "imageModelLabel1";
    private static final String sImageModelLabel2 = "imageModelLabel2";

    private static final String IMAGE_TYPE = "image/jpeg";
    private static final String URL = "http://www.google.com/";

    private Activity mActivity;
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mPropertyModelBuilder = new ShareSheetPropertyModelBuilder(null, mPackageManager);

        setUpResolveInfo(mTextResolveInfo1, "textPackage1", sTextModelLabel1);
        setUpResolveInfo(mTextResolveInfo2, "textPackage2", sTextModelLabel2);
        setUpResolveInfo(mImageResolveInfo1, "imagePackage1", sImageModelLabel1);
        setUpResolveInfo(mImageResolveInfo2, "imagePackage1", sImageModelLabel2);
        mImageResolveInfo2.activityInfo.name = "com.google.android.gm.ComposeActivityGmailExternal";

        doReturn(ImmutableList.of(mTextResolveInfo1, mTextResolveInfo2))
                .when(mPackageManager)
                .queryIntentActivities(
                        argThat(intent -> intent.getType().equals("text/plain")), anyInt());
        doReturn(ImmutableList.of(mImageResolveInfo1, mImageResolveInfo2))
                .when(mPackageManager)
                .queryIntentActivities(
                        argThat(intent -> intent.getType().equals("image/jpeg")), anyInt());
        when(mPackageManager.getResourcesForApplication(anyString()))
                .thenReturn(mActivity.getResources());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasCorrectLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", URL).build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should contain LINK_PAGE_NOT_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
        shareExtras = new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        assertEquals("Should contain LINK_PAGE_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoUrl_hasNoLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should not contain LINK_PAGE_NOT_VISIBLE", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
        shareExtras = new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        assertEquals("Should not contain LINK_PAGE_VISIBLE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndUrlDifferentFromText_hasTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").setText("testText").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should contain TEXT.", ImmutableSet.of(ContentType.TEXT),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndTextIsNull_hasNoTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should not contain TEXT.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    getContentTypes_sharingHub15EnabledAndUrlDifferentFromText_hasHighlightedTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").setText("testText").build();
        ChromeShareExtras shareExtras =
                new ChromeShareExtras.Builder().setIsUserHighlightedText(true).build();

        assertEquals("Should contain HIGHLIGHTED_TEXT.",
                ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasImageContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "")
                                          .setFileUris(new ArrayList<>(ImmutableSet.of(Uri.EMPTY)))
                                          .setFileContentType(IMAGE_TYPE)
                                          .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should contain IMAGE.", ImmutableSet.of(ContentType.IMAGE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoFiles_hasNoImageContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType(IMAGE_TYPE).build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should not contain IMAGE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasOtherFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should contain OTHER_FILE_TYPE.",
                ImmutableSet.of(ContentType.OTHER_FILE_TYPE),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15EnabledAndNoFiles_hasNoFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType("*/*").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should not contain OTHER_FILE_TYPE.", ImmutableSet.of(),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Enabled_hasMultipleContentTypes() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL)
                        .setText("testText")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals("Should contain correct content types.",
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.OTHER_FILE_TYPE,
                        ContentType.TEXT, ContentType.LINK_AND_TEXT),
                ShareSheetPropertyModelBuilder.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void getContentTypes_sharingHub15Disabled_returnsAllContentTypes() {
        assertEquals("Should contain all content types.",
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES,
                ShareSheetPropertyModelBuilder.getContentTypes(null, null));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void selectThirdPartyApps_sharingHub15Disabled_returnsTextSharingModels() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();

        List<PropertyModel> propertyModels =
                mPropertyModelBuilder.selectThirdPartyApps(null, new HashSet<>(), shareParams,
                        /*saveLastUsed=*/false, /*WindowAndroid=*/null, /*shareStartTime=*/0);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(sTextModelLabel1, sTextModelLabel2));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    selectThirdPartyApps_sharingHub15Disabled_returnsTextSharingModelsExcludeChromePackage() {
        String chromePackage = ContextUtils.getApplicationContext().getPackageName();
        try {
            setUpResolveInfo(mTextResolveInfo3, chromePackage, sTextModelLabel2);
        } catch (PackageManager.NameNotFoundException e) {
            return;
        }
        doReturn(ImmutableList.of(mTextResolveInfo1, mTextResolveInfo2, mTextResolveInfo3))
                .when(mPackageManager)
                .queryIntentActivities(
                        argThat(intent -> intent.getType().equals("text/plain")), anyInt());

        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();

        List<PropertyModel> propertyModels =
                mPropertyModelBuilder.selectThirdPartyApps(null, new HashSet<>(), shareParams,
                        /*saveLastUsed=*/false, /*WindowAndroid=*/null, /*shareStartTime=*/0);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(sTextModelLabel1, sTextModelLabel2));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void selectThirdPartyApps_sharingHub15EnabledAndLinkShare_returnsTextSharingModels() {
        ShareParams shareParams = new ShareParams.Builder(null, "", URL).build();

        List<PropertyModel> propertyModels = mPropertyModelBuilder.selectThirdPartyApps(null,
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE), shareParams, /*saveLastUsed=*/false,
                /*WindowAndroid=*/null,
                /*shareStartTime=*/0);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(sTextModelLabel1, sTextModelLabel2));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void selectThirdPartyApps_sharingHub15EnabledAndImageShare_returnsImageSharingModels() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType("image/jpeg").build();

        List<PropertyModel> propertyModels = mPropertyModelBuilder.selectThirdPartyApps(null,
                ImmutableSet.of(ContentType.IMAGE), shareParams, /*saveLastUsed=*/false,
                /*WindowAndroid=*/null, /*shareStartTime=*/0);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(sImageModelLabel2, sImageModelLabel1));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
    public void
    selectThirdPartyApps_sharingHub15EnabledAndLinkImageShare_returnsTextAndImageSharingModels() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL).setFileContentType("image/jpeg").build();

        List<PropertyModel> propertyModels = mPropertyModelBuilder.selectThirdPartyApps(null,
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE, ContentType.IMAGE), shareParams,
                /*saveLastUsed=*/false, /*WindowAndroid=*/null, /*shareStartTime=*/0);

        assertEquals("Incorrect number of property models.", 4, propertyModels.size());
        assertModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(
                        sImageModelLabel2, sTextModelLabel1, sTextModelLabel2, sImageModelLabel1));
    }

    private void setUpResolveInfo(ResolveInfo resolveInfo, String packageName, String label)
            throws PackageManager.NameNotFoundException {
        resolveInfo.activityInfo =
                mActivity.getPackageManager().getActivityInfo(mActivity.getComponentName(), 0);
        resolveInfo.activityInfo.packageName = packageName;
        when(resolveInfo.loadLabel(any())).thenReturn(label);
        when(resolveInfo.loadIcon(any())).thenReturn(null);
        resolveInfo.icon = 0;
    }

    private void assertModelsAreInTheRightOrder(
            List<PropertyModel> propertyModels, List<String> expectedOrder) {
        ImmutableList.Builder<String> actualLabelOrder = ImmutableList.builder();
        for (PropertyModel propertyModel : propertyModels) {
            actualLabelOrder.add(propertyModel.get(ShareSheetItemViewProperties.LABEL));
        }
        assertEquals(
                "Property models in the wrong order.", expectedOrder, actualLabelOrder.build());
    }
}
