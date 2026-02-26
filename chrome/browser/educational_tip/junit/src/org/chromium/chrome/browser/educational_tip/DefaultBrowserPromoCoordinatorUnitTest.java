// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoBottomSheetContent;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.widget.ButtonCompat;

/** Test relating to {@link DefaultBrowserPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class DefaultBrowserPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private SetupListManager mSetupListManager;

    private DefaultBrowserPromoCoordinator mDefaultBrowserPromoCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mActionDelegate.getContext()).thenReturn(mActivity);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
        SetupListManager.setInstanceForTesting(mSetupListManager);

        mDefaultBrowserPromoCoordinator =
                new DefaultBrowserPromoCoordinator(mOnModuleClickedCallback, mActionDelegate);
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromoCardBottomSheet_NonSetupList() {
        when(mSetupListManager.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO))
                .thenReturn(false);

        mDefaultBrowserPromoCoordinator.onCardClicked();

        // Should use bottom sheet for non-setup-list items.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mActionDelegate, never()).maybeShowDefaultBrowserPromoWithRoleManager();
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromoCardBottomSheet_RoleManagerFails() {
        when(mSetupListManager.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO))
                .thenReturn(true);
        when(mActionDelegate.maybeShowDefaultBrowserPromoWithRoleManager()).thenReturn(false);

        mDefaultBrowserPromoCoordinator.onCardClicked();

        // Should fallback to bottom sheet.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        DefaultBrowserPromoBottomSheetContent defaultBrowserBottomSheetContent =
                mDefaultBrowserPromoCoordinator.getDefaultBrowserBottomSheetContent();
        ButtonCompat bottomSheetButton =
                defaultBrowserBottomSheetContent
                        .getContentView()
                        .findViewById(org.chromium.chrome.browser.educational_tip.R.id.button);

        bottomSheetButton.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean(), anyInt());
        verify(mOnModuleClickedCallback).run();
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromoCard_RoleManagerSucceeds() {
        when(mSetupListManager.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO))
                .thenReturn(true);
        when(mActionDelegate.maybeShowDefaultBrowserPromoWithRoleManager()).thenReturn(true);

        mDefaultBrowserPromoCoordinator.onCardClicked();

        // Should NOT show bottom sheet.
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mOnModuleClickedCallback).run();
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromoCard_MarksComplete() {
        when(mSetupListManager.isSetupListModule(ModuleType.DEFAULT_BROWSER_PROMO))
                .thenReturn(true);

        mDefaultBrowserPromoCoordinator.onCardClicked();

        // Verify it marks the module as completed.
        verify(mSetupListManager)
                .setModuleCompleted(ModuleType.DEFAULT_BROWSER_PROMO, /* silent= */ false);
    }
}
