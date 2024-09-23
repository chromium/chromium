// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY})
public class PageSummaryButtonControllerUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private PageInfoSharingController mPageInfoSharingController;
    @Mock private Tab mTab;

    @Test
    public void testButtonData() {

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    PageSummaryButtonController controller =
                            new PageSummaryButtonController(
                                    activity,
                                    mBottomSheetController,
                                    mModalDialogManager,
                                    () -> mTab,
                                    mPageInfoSharingController);

                    ButtonData buttonData = controller.get(mTab);

                    assertNotNull(buttonData);
                    assertTrue(buttonData.canShow());
                    assertTrue(buttonData.isEnabled());
                    assertEquals(
                            AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                            buttonData.getButtonSpec().getButtonVariant());
                });
    }

    @Test
    public void testButtonClick() {
        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    PageSummaryButtonController controller =
                            new PageSummaryButtonController(
                                    activity,
                                    mBottomSheetController,
                                    mModalDialogManager,
                                    () -> mTab,
                                    mPageInfoSharingController);

                    ButtonData buttonData = controller.get(mTab);

                    buttonData.getButtonSpec().getOnClickListener().onClick(null);

                    verify(mPageInfoSharingController)
                            .sharePageInfo(activity, mBottomSheetController, null, mTab);
                });
    }
}
