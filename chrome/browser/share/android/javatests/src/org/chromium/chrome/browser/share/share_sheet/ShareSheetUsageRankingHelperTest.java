// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests {@link ShareSheetUsageRankingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ShareSheetUsageRankingHelperTest {
    private static final String MOCK_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ShareSheetBottomSheetContent mBottomSheet;
    @Mock private ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    @Mock private Set<Integer> mContentTypes;
    @Mock private Profile mProfile;
    @Mock private ShareParams.TargetChosenCallback mTargetChosenCallback;
    @Mock private WindowAndroid mWindow;

    private Activity mActivity;
    private ShareParams mParams;
    private ShareSheetUsageRankingHelper mShareSheetUsageRankingHelper;
    private @LinkGeneration int mLinkGenerationStatusForMetrics = LinkGeneration.MAX;
    private LinkToggleMetricsDetails mLinkToggleMetricsDetails =
            new LinkToggleMetricsDetails(LinkToggleState.COUNT, DetailedContentType.NOT_SPECIFIED);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        mActivity = Robolectric.setupActivity(Activity.class);
        when(mWindow.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindow.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(anyString()))
                .thenReturn(new GURL(MOCK_URL));
        when(mContentTypes.contains(ShareContentTypeHelper.ContentType.IMAGE)).thenReturn(true);

        mParams =
                new ShareParams.Builder(mWindow, "title", MOCK_URL)
                        .setCallback(mTargetChosenCallback)
                        .build();

        mShareSheetUsageRankingHelper =
                new ShareSheetUsageRankingHelper(
                        mBottomSheetController,
                        mBottomSheet,
                        /* shareStartTime= */ 1234,
                        mLinkGenerationStatusForMetrics,
                        mLinkToggleMetricsDetails,
                        mPropertyModelBuilder,
                        mProfile);
    }

    @Test
    @SmallTest
    public void testCreateThirdPartyPropertyModelsFromUsageRanking() throws TimeoutException {
        List<String> targets = new ArrayList<String>();
        targets.add("$more");
        targets.add("$more");
        final AtomicReference<List<PropertyModel>> resultPropertyModels = new AtomicReference<>();
        CallbackHelper helper = new CallbackHelper();

        mShareSheetUsageRankingHelper.setTargetsForTesting(targets);
        mShareSheetUsageRankingHelper.createThirdPartyPropertyModelsFromUsageRanking(
                mActivity,
                mParams,
                mContentTypes,
                /* saveLastUsed= */ false,
                models -> {
                    resultPropertyModels.set(models);
                    helper.notifyCalled();
                });
        helper.waitForOnly();
        List<PropertyModel> propertyModels = resultPropertyModels.get();

        assertEquals("Incorrect number of property models.", 2, propertyModels.size());
        assertEquals(
                "First property model isn't More.",
                mActivity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        assertEquals(
                "Second property model isn't More.",
                mActivity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
    }

    @Test
    @SmallTest
    public void testClickMoreRemovesCallback() throws TimeoutException {
        List<String> targets = new ArrayList<String>();
        targets.add("$more");
        final AtomicReference<List<PropertyModel>> resultPropertyModels = new AtomicReference<>();
        CallbackHelper helper = new CallbackHelper();

        mShareSheetUsageRankingHelper.setTargetsForTesting(targets);
        mShareSheetUsageRankingHelper.createThirdPartyPropertyModelsFromUsageRanking(
                mActivity,
                mParams,
                mContentTypes,
                /* saveLastUsed= */ false,
                models -> {
                    resultPropertyModels.set(models);
                    helper.notifyCalled();
                });
        helper.waitForOnly();
        List<PropertyModel> propertyModels = resultPropertyModels.get();

        View.OnClickListener onClickListener =
                propertyModels.get(0).get(ShareSheetItemViewProperties.CLICK_LISTENER);

        assertNotNull("Callback should not be null before pressing More", mParams.getCallback());
        onClickListener.onClick(null);
        assertNull("Callback should be null after pressing More", mParams.getCallback());
    }

    ResolveInfo resolveInfoForPackage(String name) {
        ResolveInfo info = new ResolveInfo();
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = name;
        info.activityInfo.name = "foo";
        return info;
    }

    @Test
    @SmallTest
    public void testFilteringRemovesCtsShims() {
        List<ResolveInfo> infos =
                List.of(
                        resolveInfoForPackage("org.chromium.a"),
                        resolveInfoForPackage("com.android.cts.ctsshim"),
                        resolveInfoForPackage("org.chromium.b"),
                        resolveInfoForPackage("com.android.cts.priv.ctsshim"),
                        resolveInfoForPackage("org.chromium.c"));

        List<ResolveInfo> result =
                ShareSheetUsageRankingHelper.filterOutBlocklistedResolveInfos(infos);

        assertEquals(3, result.size());
        assertEquals("org.chromium.a", result.get(0).activityInfo.packageName);
        assertEquals("org.chromium.b", result.get(1).activityInfo.packageName);
        assertEquals("org.chromium.c", result.get(2).activityInfo.packageName);
    }
}
