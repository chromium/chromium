// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anySet;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests {@link ShareSheetCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION})
@LooperMode(LooperMode.Mode.LEGACY)
public final class ShareSheetCoordinatorTest {
    private static final String MOCK_URL = JUnitTestGURLs.EXAMPLE_URL;

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock
    private BottomSheetController mController;
    @Mock
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    @Mock
    private ShareParams.TargetChosenCallback mTargetChosenCallback;
    @Mock
    private Supplier<Tab> mTabProvider;
    @Mock
    private WindowAndroid mWindow;
    @Mock
    private Profile mProfile;
    @Mock
    Tracker mTracker;

    private Activity mActivity;
    private ShareParams mParams;
    private ShareSheetCoordinator mShareSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        mActivity = Robolectric.setupActivity(Activity.class);
        PropertyModel testModel1 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel1")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .build();
        PropertyModel testModel2 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel2")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .build();

        ArrayList<PropertyModel> thirdPartyPropertyModels =
                new ArrayList<>(Arrays.asList(testModel1, testModel2));
        when(mWindow.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mPropertyModelBuilder.selectThirdPartyApps(
                     any(), anySet(), any(), anyBoolean(), anyLong(), anyInt(), any()))
                .thenReturn(thirdPartyPropertyModels);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(anyString()))
                .thenReturn(JUnitTestGURLs.getGURL(MOCK_URL));
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);

        mParams = new ShareParams.Builder(mWindow, "title", MOCK_URL)
                          .setCallback(mTargetChosenCallback)
                          .build();
        mShareSheetCoordinator = new ShareSheetCoordinator(mController, mLifecycleDispatcher,
                mTabProvider, mPropertyModelBuilder, null, null, false, null, null);
    }

    @Test
    @SmallTest
    public void disableFirstPartyFeatures() {
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();

        List<PropertyModel> propertyModels = mShareSheetCoordinator.createFirstPartyPropertyModels(
                mActivity, mParams, /*chromeShareExtras=*/null,
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST);
        assertEquals("Property model list should be empty.", 0, propertyModels.size());
    }

    @Test
    @SmallTest
    public void testCreateThirdPartyPropertyModels() throws TimeoutException {
        final AtomicReference<List<PropertyModel>> resultPropertyModels = new AtomicReference<>();
        CallbackHelper helper = new CallbackHelper();
        mShareSheetCoordinator.createThirdPartyPropertyModels(mActivity, mParams,
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                /*saveLastUsed=*/false, models -> {
                    resultPropertyModels.set(models);
                    helper.notifyCalled();
                });
        helper.waitForFirst();
        List<PropertyModel> propertyModels = resultPropertyModels.get();

        assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        assertEquals("First property model isn't testModel1.", "testModel1",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Second property model isn't testModel2.", "testModel2",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Third property model isn't More.",
                mActivity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
    }

    @Test
    @SmallTest
    public void testClickMoreRemovesCallback() throws TimeoutException {
        final AtomicReference<List<PropertyModel>> resultPropertyModels = new AtomicReference<>();
        CallbackHelper helper = new CallbackHelper();
        mShareSheetCoordinator.createThirdPartyPropertyModels(mActivity, mParams,
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                /*saveLastUsed=*/false, models -> {
                    resultPropertyModels.set(models);
                    helper.notifyCalled();
                });
        helper.waitForFirst();
        List<PropertyModel> propertyModels = resultPropertyModels.get();

        View.OnClickListener onClickListener =
                propertyModels.get(2).get(ShareSheetItemViewProperties.CLICK_LISTENER);

        assertNotNull("Callback should not be null before pressing More", mParams.getCallback());
        onClickListener.onClick(null);
        assertNull("Callback should be null after pressing More", mParams.getCallback());
    }
}
