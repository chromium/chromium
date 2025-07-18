// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ArchivedTabsAutoDeletePromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabsAutoDeletePromoCoordinatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private TabArchiveSettings mMockTabArchiveSettings;
    @Mock private SettingsNavigation mMockSettingsNavigation;

    @Captor
    private ArgumentCaptor<ArchivedTabsAutoDeletePromoSheetContent>
            mBottomSheetContentArgumentCaptor;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverArgumentCaptor;

    private Activity mActivity;
    private ArchivedTabsAutoDeletePromoCoordinator mCoordinator;
    private PropertyModel mCoordinatorModel;

    @Before
    public void setUp() {
        MockitoJUnit.rule();
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mCoordinator =
                new ArchivedTabsAutoDeletePromoCoordinator(
                        mActivity, mMockBottomSheetController, mMockTabArchiveSettings);
        mCoordinatorModel = mCoordinator.getModelForTesting();
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    /**
     * Helper to simulate a successful request to show content and get the Coordinator's observer.
     *
     * @return The BottomSheetObserver instance used by the Coordinator.
     */
    private BottomSheetObserver simulateShowSuccessAndGetObserver() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(true);
        mCoordinator.showPromo();
        verify(mMockBottomSheetController)
                .addObserver(mBottomSheetObserverArgumentCaptor.capture());
        BottomSheetObserver coordinatorObserver = mBottomSheetObserverArgumentCaptor.getValue();
        assertNotNull(
                "Coordinator's observer should be set after successful show.", coordinatorObserver);
        verify(mMockBottomSheetController).addObserver(eq(coordinatorObserver));
        return coordinatorObserver;
    }

    /**
     * Helper to simulate the sheet being closed.
     *
     * @param observer The observer whose onSheetClosed method to call.
     * @param reason The reason for the sheet closure.
     */
    private void simulateSheetClose(
            @Nullable BottomSheetObserver observer, @StateChangeReason int reason) {
        if (observer != null) {
            observer.onSheetClosed(reason);
        }
    }

    @Test
    public void testShowPromo_Success_ShowsAndObserves() {
        simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        assertNotNull(mBottomSheetContentArgumentCaptor.getValue());
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
    }

    @Test
    public void testShowPromo_Fails_NoSettingsChangeOrObserver() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.showPromo();
        verify(mMockBottomSheetController)
                .requestShowContent(any(BottomSheetContent.class), eq(true));
        verify(mMockBottomSheetController, never()).addObserver(any(BottomSheetObserver.class));
        assertFalse(mCoordinator.isSheetCurrentlyManagedForTesting());
        verify(mMockTabArchiveSettings, never()).setAutoDeleteEnabled(anyBoolean());
        verify(mMockTabArchiveSettings, never()).setAutoDeleteDecisionMade(anyBoolean());
    }

    @Test
    public void testYesButton_EnablesAutoDelete_AndSetsDecisionMade() {
        BottomSheetObserver coordinatorObserver = simulateShowSuccessAndGetObserver();
        View.OnClickListener yesListener =
                mCoordinatorModel.get(
                        ArchivedTabsAutoDeletePromoProperties.ON_YES_BUTTON_CLICK_LISTENER);
        yesListener.onClick(null);
        simulateSheetClose(coordinatorObserver, StateChangeReason.INTERACTION_COMPLETE);

        verify(mMockTabArchiveSettings).setAutoDeleteEnabled(true);
        verify(mMockTabArchiveSettings).setAutoDeleteDecisionMade(true);
    }

    @Test
    public void testNoButton_DisablesAutoDelete_AndSetsDecisionMade() {
        BottomSheetObserver coordinatorObserver = simulateShowSuccessAndGetObserver();
        View.OnClickListener noListener =
                mCoordinatorModel.get(
                        ArchivedTabsAutoDeletePromoProperties.ON_NO_BUTTON_CLICK_LISTENER);
        mCoordinator.setSettingsNavigationForTesting(mMockSettingsNavigation);
        noListener.onClick(null);
        simulateSheetClose(coordinatorObserver, StateChangeReason.INTERACTION_COMPLETE);

        // We set auto-delete to true before navigating to the settings page.
        verify(mMockTabArchiveSettings).setAutoDeleteEnabled(true);
        verify(mMockTabArchiveSettings).setAutoDeleteDecisionMade(true);
        verify(mMockSettingsNavigation)
                .startSettings(eq(mActivity), eq(TabArchiveSettingsFragment.class));
    }

    @Test
    public void testSwipeDismiss_EnablesAutoDelete_AndSetsDecisionMade() {
        BottomSheetObserver coordinatorObserver = simulateShowSuccessAndGetObserver();
        simulateSheetClose(coordinatorObserver, StateChangeReason.SWIPE);

        verify(mMockTabArchiveSettings).setAutoDeleteEnabled(true);
        verify(mMockTabArchiveSettings).setAutoDeleteDecisionMade(true);
    }

    @Test
    public void testBackPressDismiss_EnablesAutoDelete_AndSetsDecisionMade() {
        BottomSheetObserver coordinatorObserver = simulateShowSuccessAndGetObserver();
        verify(mMockBottomSheetController)
                .requestShowContent(mBottomSheetContentArgumentCaptor.capture(), eq(true));
        ArchivedTabsAutoDeletePromoSheetContent sheetContent =
                mBottomSheetContentArgumentCaptor.getValue();
        assertNotNull(sheetContent);
        sheetContent.handleBackPress();
        simulateSheetClose(coordinatorObserver, StateChangeReason.BACK_PRESS);

        verify(mMockTabArchiveSettings).setAutoDeleteEnabled(true);
        verify(mMockTabArchiveSettings).setAutoDeleteDecisionMade(true);
    }

    @Test
    public void testDestroy_WhenShown_HidesAndFinalizes() {
        simulateShowSuccessAndGetObserver();
        assertTrue(mCoordinator.isSheetCurrentlyManagedForTesting());
        mCoordinator.destroy();

        verify(mMockBottomSheetController)
                .hideContent(
                        any(ArchivedTabsAutoDeletePromoSheetContent.class),
                        eq(false),
                        eq(StateChangeReason.NONE));
        verify(mMockTabArchiveSettings).setAutoDeleteEnabled(true);
        verify(mMockTabArchiveSettings).setAutoDeleteDecisionMade(true);
    }

    @Test
    public void testDestroy_WhenNotShown_NoActionOrSettingsChange() {
        when(mMockBottomSheetController.requestShowContent(any(BottomSheetContent.class), eq(true)))
                .thenReturn(false);
        mCoordinator.showPromo();
        mCoordinator.destroy();

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
        verify(mMockTabArchiveSettings, never()).setAutoDeleteEnabled(anyBoolean());
        verify(mMockTabArchiveSettings, never()).setAutoDeleteDecisionMade(anyBoolean());
    }
}
