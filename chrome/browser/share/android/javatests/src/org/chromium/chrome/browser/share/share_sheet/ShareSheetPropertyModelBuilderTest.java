// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import androidx.test.filters.MediumTest;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit tests {@link ShareSheetPropertyModelBuilder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ShareSheetPropertyModelBuilderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private PackageManager mPackageManager;
    @Mock private Profile mProfile;
    @Mock private ResolveInfo mTextResolveInfo1;
    @Mock private ResolveInfo mTextResolveInfo2;
    @Mock private ResolveInfo mImageResolveInfo1;
    @Mock private ResolveInfo mImageResolveInfo2;

    private static final String TEXT_MODEL_LABEL_1 = "textModelLabel1";
    private static final String TEXT_MODEL_LABEL_2 = "textModelLabel2";
    private static final String IMAGE_MODEL_LABEL_1 = "imageModelLabel1";
    private static final String IMAGE_MODEL_LABEL_2 = "imageModelLabel2";

    private static final String IMAGE_TYPE = "image/jpeg";
    private static final String URL = "http://www.google.com/";

    private Activity mActivity;
    private TestContext mTestContext;
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    class TestContext extends ContextWrapper {
        public TestContext(Context base) {
            super(base);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(mPackageManager);
        }
    }

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mPropertyModelBuilder = new ShareSheetPropertyModelBuilder(null, mPackageManager, mProfile);

        setUpResolveInfo(mTextResolveInfo1, "textPackage1", TEXT_MODEL_LABEL_1);
        setUpResolveInfo(mTextResolveInfo2, "textPackage2", TEXT_MODEL_LABEL_2);
        setUpResolveInfo(mImageResolveInfo1, "imagePackage1", IMAGE_MODEL_LABEL_1);
        setUpResolveInfo(mImageResolveInfo2, "imagePackage1", IMAGE_MODEL_LABEL_2);
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
    public void getContentTypes_hasCorrectLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", URL).build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain LINK_PAGE_NOT_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
        shareExtras = new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        assertEquals(
                "Should contain LINK_PAGE_VISIBLE.",
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_NoUrl_hasNoLinkContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should not contain LINK_PAGE_NOT_VISIBLE",
                ImmutableSet.of(),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
        shareExtras = new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        assertEquals(
                "Should not contain LINK_PAGE_VISIBLE.",
                ImmutableSet.of(),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_UrlDifferentFromText_hasTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").setText("testText").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain TEXT.",
                ImmutableSet.of(ContentType.TEXT),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_TextIsNull_hasNoTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should not contain TEXT.",
                ImmutableSet.of(),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_UrlDifferentFromText_hasHighlightedTextContentType() {
        ShareParams shareParams = new ShareParams.Builder(null, "", "").setText("testText").build();
        ChromeShareExtras shareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();

        assertEquals(
                "Should contain HIGHLIGHTED_TEXT.",
                ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_hasImageContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "")
                        .setSingleImageUri(Uri.EMPTY)
                        .setFileContentType(IMAGE_TYPE)
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain IMAGE.",
                ImmutableSet.of(ContentType.IMAGE),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_NoFiles_hasNoImageContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType(IMAGE_TYPE).build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should not contain IMAGE.",
                ImmutableSet.of(),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_hasOtherFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain OTHER_FILE_TYPE.",
                ImmutableSet.of(ContentType.OTHER_FILE_TYPE),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_hasImageAndLink_AndPage() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL)
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain IMAGE_AND_LINK and LINK_PAGE_NOT_VISIBLE.",
                ImmutableSet.of(ContentType.IMAGE_AND_LINK, ContentType.LINK_PAGE_NOT_VISIBLE),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_hasImageAndLink_NoPage() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL)
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("image/png")
                        .build();
        ChromeShareExtras shareExtras =
                new ChromeShareExtras.Builder().setSkipPageSharingActions(true).build();

        assertEquals(
                "Should contain IMAGE_AND_LINK.",
                ImmutableSet.of(ContentType.IMAGE_AND_LINK),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_NoFiles_hasNoFileContentType() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType("*/*").build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should not contain OTHER_FILE_TYPE.",
                ImmutableSet.of(),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void getContentTypes_hasMultipleContentTypes() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL)
                        .setText("testText")
                        .setFileUris(new ArrayList<>(ImmutableList.of(Uri.EMPTY, Uri.EMPTY)))
                        .setFileContentType("*/*")
                        .build();
        ChromeShareExtras shareExtras = new ChromeShareExtras.Builder().build();

        assertEquals(
                "Should contain correct content types.",
                ImmutableSet.of(
                        ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.OTHER_FILE_TYPE,
                        ContentType.TEXT,
                        ContentType.LINK_AND_TEXT),
                ShareContentTypeHelper.getContentTypes(shareParams, shareExtras));
    }

    @Test
    @MediumTest
    public void selectThirdPartyApps_LinkShare_returnsTextSharingModels() {
        ShareParams shareParams = new ShareParams.Builder(null, "", URL).build();

        List<PropertyModel> propertyModels =
                mPropertyModelBuilder.selectThirdPartyApps(
                        null,
                        ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                        shareParams,
                        /* saveLastUsed= */ false,
                        /* shareStartTime= */ 0,
                        /* linkGenerationStatusForMetrics= */ LinkGeneration.MAX,
                        /* linkToggleMetricsDetails= */ null);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(TEXT_MODEL_LABEL_1, TEXT_MODEL_LABEL_2));
    }

    @Test
    @MediumTest
    public void selectThirdPartyApps_ImageShare_returnsImageSharingModels() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", "").setFileContentType("image/jpeg").build();

        List<PropertyModel> propertyModels =
                mPropertyModelBuilder.selectThirdPartyApps(
                        null,
                        ImmutableSet.of(ContentType.IMAGE),
                        shareParams,
                        /* saveLastUsed= */ false,
                        /* shareStartTime= */ 0,
                        /* linkGenerationStatusForMetrics= */ LinkGeneration.MAX,
                        /* linkToggleMetricsDetails= */ null);

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels, ImmutableList.of(IMAGE_MODEL_LABEL_2, IMAGE_MODEL_LABEL_1));
    }

    @Test
    @MediumTest
    public void selectThirdPartyApps_LinkImageShare_returnsTextAndImageSharingModels() {
        ShareParams shareParams =
                new ShareParams.Builder(null, "", URL).setFileContentType("image/jpeg").build();

        List<PropertyModel> propertyModels =
                mPropertyModelBuilder.selectThirdPartyApps(
                        null,
                        ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE, ContentType.IMAGE),
                        shareParams,
                        /* saveLastUsed= */ false,
                        /* shareStartTime= */ 0,
                        /* linkGenerationStatusForMetrics= */ LinkGeneration.MAX,
                        /* linkToggleMetricsDetails= */ null);

        assertEquals("Incorrect number of property models.", 4, propertyModels.size());
        assertModelsAreInTheRightOrder(
                propertyModels,
                ImmutableList.of(
                        IMAGE_MODEL_LABEL_2,
                        TEXT_MODEL_LABEL_1,
                        TEXT_MODEL_LABEL_2,
                        IMAGE_MODEL_LABEL_1));
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
