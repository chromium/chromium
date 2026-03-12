// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS_ON_CLICK;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_TITLE;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProviderFactory;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link EducationalTipSetupListBottomSheetCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipSetupListBottomSheetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    @Mock
    private Supplier<List<EducationalTipSetupListBottomSheetItem>>
            mEducationalTipCardProviderSupplier;

    private Context mContext;
    private List<EducationalTipSetupListBottomSheetItem>
            mListOfEducationalTipSetupListBottomSheetItem;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
        mListOfEducationalTipSetupListBottomSheetItem = createListOfEducationalTipBottomSheetItem();
        when(mEducationalTipCardProviderSupplier.get())
                .thenReturn(mListOfEducationalTipSetupListBottomSheetItem);

        List<Integer> moduleTypeList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            moduleTypeList.add(ModuleType.DEFAULT_BROWSER_PROMO);
        }
        SetupListModuleUtils.setRankedModuleTypesForTesting(moduleTypeList);
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        EducationalTipSetupListBottomSheetCoordinator
                educationalTipSetupListBottomSheetCoordinator =
                        new EducationalTipSetupListBottomSheetCoordinator(
                                mActionDelegate, mEducationalTipCardProviderSupplier);
        PropertyModel model = educationalTipSetupListBottomSheetCoordinator.getModelForTesting();
        educationalTipSetupListBottomSheetCoordinator.showBottomSheet();

        Assert.assertEquals(
                "Bottom sheet title should be default",
                model.get(BOTTOM_SHEET_TITLE),
                mContext.getString(R.string.get_the_most_out_of_chrome));
        Assert.assertEquals(
                "Bottom sheet description should be set",
                model.get(BOTTOM_SHEET_DESCRIPTION),
                mContext.getString(R.string.educational_tip_see_more_bottom_sheet_description));
        Assert.assertEquals(
                "Bottom sheet list items should be set",
                model.get(BOTTOM_SHEET_LIST_ITEMS),
                mListOfEducationalTipSetupListBottomSheetItem);
        verify(mBottomSheetController).requestShowContent(any(), /* animate= */ eq(true));
    }

    @Test
    @SmallTest
    public void testDismissBottomSheet() {
        EducationalTipSetupListBottomSheetCoordinator
                educationalTipSetupListBottomSheetCoordinator =
                        new EducationalTipSetupListBottomSheetCoordinator(
                                mActionDelegate, mEducationalTipCardProviderSupplier);
        PropertyModel model = educationalTipSetupListBottomSheetCoordinator.getModelForTesting();

        // 1. Verify dismissal with animation.
        educationalTipSetupListBottomSheetCoordinator.dismissBottomSheet(true);
        verify(mBottomSheetController).hideContent(any(), eq(true));

        // 2. Verify dismissal without animation (triggered by item click).
        Runnable dismissalRunnable = model.get(BOTTOM_SHEET_LIST_ITEMS_ON_CLICK);
        Assert.assertNotNull(dismissalRunnable);
        dismissalRunnable.run();
        verify(mBottomSheetController).hideContent(any(), eq(false));
    }

    private List<EducationalTipSetupListBottomSheetItem>
            createListOfEducationalTipBottomSheetItem() {
        List<EducationalTipSetupListBottomSheetItem> output = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            EducationalTipCardProvider provider =
                    EducationalTipCardProviderFactory.createInstance(
                            ModuleType.DEFAULT_BROWSER_PROMO,
                            () -> {},
                            null,
                            mActionDelegate,
                            () -> {});
            output.add(new EducationalTipSetupListBottomSheetItem(provider, null));
        }
        return output;
    }
}
