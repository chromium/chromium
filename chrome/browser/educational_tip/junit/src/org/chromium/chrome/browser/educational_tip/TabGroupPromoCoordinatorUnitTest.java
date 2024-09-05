// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupPromoCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabGridIphDialogCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link TabGroupPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class TabGroupPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private Runnable mShowTabSwitcherRunnable;
    @Mock private Supplier<ViewGroup> mParentViewSupplier;
    @Mock private ViewGroup mParentView;
    @Mock private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;

    private ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplier;

    private TabGroupPromoCoordinator mTabGroupPromoCoordinator;

    @Before
    public void setUp() {
        mModalDialogManagerSupplier = new ObservableSupplierImpl<>();
        mModalDialogManagerSupplier.set(mModalDialogManager);

        when(mParentViewSupplier.get()).thenReturn(mParentView);

        mTabGroupPromoCoordinator =
                new TabGroupPromoCoordinator(
                        mContext,
                        mOnModuleClickedCallback,
                        mModalDialogManagerSupplier,
                        mShowTabSwitcherRunnable,
                        mParentViewSupplier);
        mTabGroupPromoCoordinator.setTabGridIphDialogCoordinatorForTesting(
                mTabGridIphDialogCoordinator);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testClickTabGroupPromoCard() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        mTabGroupPromoCoordinator.onCardClicked();
        verify(mTabGridIphDialogCoordinator).showIph();
        verify(mShowTabSwitcherRunnable).run();
        verify(mOnModuleClickedCallback).run();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mTabGroupPromoCoordinator.destroy();
        verify(mTabGridIphDialogCoordinator).setParentView(eq(null));
    }
}
