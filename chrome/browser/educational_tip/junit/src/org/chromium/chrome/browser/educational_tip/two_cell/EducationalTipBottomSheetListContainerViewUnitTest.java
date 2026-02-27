// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link EducationalTipBottomSheetListContainerView} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipBottomSheetListContainerViewUnitTest {
    private static final int EDUCATIONAL_TIP_MODULES_SIZE = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EducationalTipCardProvider mEducationalTipCardProvider;

    private EducationalTipBottomSheetListContainerView mContainerView;
    private List<EducationalTipBottomSheetItem> mListOfEducationalTipBottomSheetItem;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mContainerView = new EducationalTipBottomSheetListContainerView(context, null);
        mListOfEducationalTipBottomSheetItem = createListOfEducationalTipBottomSheetItem();

        List<Integer> moduleTypeList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            moduleTypeList.add(ModuleDelegate.ModuleType.DEFAULT_BROWSER_PROMO);
        }
        SetupListModuleUtils.setRankedModuleTypesForTesting(moduleTypeList);
    }

    @Test
    public void testRenderSetUpList_numberOfListItemsCreated() {
        mContainerView.renderSetUpList(mListOfEducationalTipBottomSheetItem);
        Assert.assertEquals(
                "All educational tip modules should be added",
                EDUCATIONAL_TIP_MODULES_SIZE,
                mContainerView.getChildCount());

        mContainerView.renderSetUpList(mListOfEducationalTipBottomSheetItem);
        Assert.assertEquals(
                "Previous educational tip list items should be destroyed",
                EDUCATIONAL_TIP_MODULES_SIZE,
                mContainerView.getChildCount());
    }

    @Test
    public void testEducationalTipCardProviderInRenderSetUpList() {
        mContainerView.renderSetUpList(
                List.of(new EducationalTipBottomSheetItem(mEducationalTipCardProvider, null)));
        verify(mEducationalTipCardProvider, times(1)).getCardImage();
        verify(mEducationalTipCardProvider, times(1)).getCardTitle();
        verify(mEducationalTipCardProvider, times(1)).getCardDescription();
    }

    @Test
    public void testListItemOnClick() {
        Runnable mockDismissRunnable = mock(Runnable.class);
        mContainerView.setDismissBottomSheet(mockDismissRunnable);
        mContainerView.renderSetUpList(
                List.of(new EducationalTipBottomSheetItem(mEducationalTipCardProvider, null)));

        mContainerView.getChildAt(0).performClick();

        verify(mEducationalTipCardProvider, times(1)).onCardClicked();
        verify(mockDismissRunnable, times(1)).run();
    }

    private List<EducationalTipBottomSheetItem> createListOfEducationalTipBottomSheetItem() {
        List<EducationalTipBottomSheetItem> output = new ArrayList<>();
        for (int i = 0; i < EDUCATIONAL_TIP_MODULES_SIZE; i++) {
            output.add(
                    new EducationalTipBottomSheetItem(
                            mock(EducationalTipCardProvider.class), null));
        }
        return output;
    }
}
