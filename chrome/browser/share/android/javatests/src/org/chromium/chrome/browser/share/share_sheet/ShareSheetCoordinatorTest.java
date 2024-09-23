// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetCoordinatorTest.ShadowPropertyModelBuilder;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Tests {@link ShareSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
@Config(shadows = ShadowPropertyModelBuilder.class)
public final class ShareSheetCoordinatorTest {
    private static final String MOCK_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private BottomSheetController mController;
    @Mock private ShareParams.TargetChosenCallback mTargetChosenCallback;
    @Mock private Supplier<Tab> mTabProvider;
    @Mock private WindowAndroid mWindow;
    @Mock private Profile mProfile;
    @Mock Tracker mTracker;

    private Activity mActivity;
    private ShareParams mParams;
    private ShareSheetCoordinator mShareSheetCoordinator;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        Context context = ContextUtils.getApplicationContext();
        mShadowPackageManager = Shadows.shadowOf(context.getPackageManager());

        mActivity = Robolectric.setupActivity(Activity.class);
        PropertyModel testModel1 =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON, null)
                        .with(ShareSheetItemViewProperties.LABEL, "testModel1")
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                        .build();
        PropertyModel testModel2 =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON, null)
                        .with(ShareSheetItemViewProperties.LABEL, "testModel2")
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                        .build();

        ArrayList<PropertyModel> thirdPartyPropertyModels =
                new ArrayList<>(Arrays.asList(testModel1, testModel2));
        when(mWindow.getActivity()).thenReturn(new WeakReference<>(mActivity));
        ShadowPropertyModelBuilder.sThirdPartyModels = thirdPartyPropertyModels;
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(anyString()))
                .thenReturn(new GURL(MOCK_URL));
        TrackerFactory.setTrackerForTests(mTracker);

        mParams =
                new ShareParams.Builder(mWindow, "title", MOCK_URL)
                        .setCallback(mTargetChosenCallback)
                        .build();
        mShareSheetCoordinator =
                new ShareSheetCoordinator(
                        mController,
                        mLifecycleDispatcher,
                        mTabProvider,
                        null,
                        null,
                        false,
                        null,
                        mProfile,
                        null);
    }

    @After
    public void tearDown() {
        ShadowPropertyModelBuilder.sThirdPartyModels = null;
    }

    @Test
    @SmallTest
    public void disableFirstPartyFeatures() {
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();

        List<PropertyModel> propertyModels =
                mShareSheetCoordinator.createFirstPartyPropertyModels(
                        mActivity,
                        mParams,
                        /* chromeShareExtras= */ null,
                        ShareContentTypeHelper.ALL_CONTENT_TYPES_FOR_TEST);
        assertEquals("Property model list should be empty.", 0, propertyModels.size());
    }

    @Test
    @SmallTest
    public void showShareSheet_avoidThirdPartyShareOptionsOnAutomotive() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();

        ShareSheetCoordinator spyShareSheet = spy(mShareSheetCoordinator);
        doNothing().when(spyShareSheet).finishUpdateShareSheet(any(), any(), any());

        spyShareSheet.updateShareSheet(/* saveLastUsed= */ false, () -> {});

        verify(spyShareSheet, never())
                .createThirdPartyPropertyModels(any(), any(), any(), anyBoolean(), any());
        verify(spyShareSheet, atLeastOnce()).finishUpdateShareSheet(any(), any(), any());
    }

    @Test
    @SmallTest
    public void showShareSheet_createThirdPartyShareOptions() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();

        ShareSheetCoordinator spyShareSheet = spy(mShareSheetCoordinator);
        doNothing().when(spyShareSheet).finishUpdateShareSheet(any(), any(), any());

        spyShareSheet.updateShareSheet(/* saveLastUsed= */ false, () -> {});

        verify(spyShareSheet, atLeastOnce())
                .createThirdPartyPropertyModels(any(), any(), any(), anyBoolean(), any());
        verify(spyShareSheet, atLeastOnce()).finishUpdateShareSheet(any(), any(), any());
    }

    /** Helper shadow class used to inject test only property models. */
    @Implements(ShareSheetPropertyModelBuilder.class)
    public static class ShadowPropertyModelBuilder {
        /** Empty ctor for robolectric to initialize. */
        public ShadowPropertyModelBuilder() {}

        static List<PropertyModel> sThirdPartyModels;

        @Implementation
        protected List<PropertyModel> selectThirdPartyApps(
                ShareSheetBottomSheetContent bottomSheet,
                Set<Integer> contentTypes,
                ShareParams params,
                boolean saveLastUsed,
                long shareStartTime,
                @LinkGeneration int linkGenerationStatusForMetrics,
                LinkToggleMetricsDetails linkToggleMetricsDetails) {
            return sThirdPartyModels == null ? new ArrayList<>() : sThirdPartyModels;
        }
    }
}
