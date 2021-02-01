// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anySet;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests {@link ShareSheetCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ShareSheetCoordinatorTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Mock
    private BottomSheetController mController;

    @Mock
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    @Mock
    private ShareParams mParams;

    private Activity mActivity;
    private ShareSheetCoordinator mShareSheetCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        MockitoAnnotations.initMocks(this);
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
        when(mPropertyModelBuilder.selectThirdPartyApps(
                     any(), anySet(), any(), anyBoolean(), any(), anyLong()))
                .thenReturn(thirdPartyPropertyModels);

        mShareSheetCoordinator = new ShareSheetCoordinator(mController, mLifecycleDispatcher, null,
                mPropertyModelBuilder, null, null, null, false, null, null);
    }

    @Test
    @MediumTest
    public void disableFirstPartyFeatures() {
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();

        List<PropertyModel> propertyModels = mShareSheetCoordinator.createFirstPartyPropertyModels(
                mActivity, mParams, /*chromeShareExtras=*/null,
                ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES);
        assertEquals("Property model list should be empty.", 0, propertyModels.size());
    }

    @Test
    @MediumTest
    public void testCreateThirdPartyPropertyModels() {
        List<PropertyModel> propertyModels = mShareSheetCoordinator.createThirdPartyPropertyModels(
                mActivity, mParams, ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES,
                /*saveLastUsed=*/false);

        assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        assertEquals("First property model isn't testModel1.", "testModel1",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Second property model isn't testModel2.", "testModel2",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Third property model isn't More.",
                mActivity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
    }
}
