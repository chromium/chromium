// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.content.Context;
import android.content.Intent;

import androidx.activity.OnBackPressedCallback;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Arrays;
import java.util.Collection;

/**
 * Robolectric tests for {@link IncognitoReauthCoordinatorFactory}.
 *
 * <p>TODO(crbug.com/40056462): Remove parameterization to improve readability of the tests.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(UNIT_TESTS)
public class IncognitoReauthCoordinatorFactoryTest {

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private final OneshotSupplierImpl<HubManager> mHubManagerSupplier = new OneshotSupplierImpl<>();

    @Mock private Context mContextMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;
    @Mock private IncognitoReauthManager mIncognitoReauthManagerMock;
    @Mock private TabSwitcherCustomViewManager mTabSwitcherCustomViewManagerMock;
    @Mock private LayoutManager mLayoutManagerMock;
    @Mock private Intent mIntentMock;
    @Mock private TabModel mIncognitoTabModelMock;
    @Mock private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock private IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegateMock;
    @Mock private IncognitoTabHost mIncognitoTabHostMock;
    @Mock private HubManager mHubManagerMock;
    @Mock private PaneManager mPaneManagerMock;

    private OnBackPressedCallback mOnBackPressedCallbackMock =
            new OnBackPressedCallback(false) {
                @Override
                public void handleOnBackPressed() {}
            };

    private final boolean mIsTabbedActivity;

    private IncognitoReauthCoordinatorFactory mIncognitoReauthCoordinatorFactory;

    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection testSuiteParameters() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public IncognitoReauthCoordinatorFactoryTest(boolean isTabbedActivity) {
        mIsTabbedActivity = isTabbedActivity;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mHubManagerMock.getPaneManager()).thenReturn(mPaneManagerMock);
        mHubManagerSupplier.set(mHubManagerMock);

        mIncognitoReauthCoordinatorFactory =
                new IncognitoReauthCoordinatorFactory(
                        mContextMock,
                        mTabModelSelectorMock,
                        mModalDialogManagerMock,
                        mIncognitoReauthManagerMock,
                        mLayoutManagerMock,
                        mHubManagerSupplier,
                        mIntentMock,
                        mIsTabbedActivity);

        if (mIsTabbedActivity) {
            mIncognitoReauthCoordinatorFactory.setTabSwitcherCustomViewManager(
                    mTabSwitcherCustomViewManagerMock);
        }

        mIncognitoReauthCoordinatorFactory.mIncognitoReauthMenuDelegateForTesting =
                mIncognitoReauthMenuDelegateMock;
    }

    @After
    public void tearDown() {
        IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHostMock);

        verifyNoMoreInteractions(
                mContextMock,
                mTabModelSelectorMock,
                mIncognitoTabModelMock,
                mModalDialogManagerMock,
                mIncognitoReauthManagerMock,
                mTabSwitcherCustomViewManagerMock,
                mLayoutManagerMock,
                mPaneManagerMock,
                mIncognitoTabHostMock);
    }

    @Test
    @SmallTest
    public void testSeeOtherTabsRunnable_IsInvokedCorrectly_LayoutNotVisible() {
        Runnable seeOtherTabsRunnable =
                mIncognitoReauthCoordinatorFactory.getSeeOtherTabsRunnable();
        if (mIsTabbedActivity) {
            when(mLayoutManagerMock.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
            doNothing().when(mTabModelSelectorMock).selectModel(/* incognito= */ false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));

            seeOtherTabsRunnable.run();

            verify(mLayoutManagerMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
            verify(mTabModelSelectorMock, times(1)).selectModel(/* incognito= */ eq(false));
            verify(mLayoutManagerMock, times(1))
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));
        } else {
            doNothing().when(mContextMock).startActivity(mIntentMock);
            seeOtherTabsRunnable.run();
            verify(mContextMock, times(1)).startActivity(mIntentMock);
        }
    }

    @Test
    @SmallTest
    public void testSeeOtherTabsRunnable_IsInvokedCorrectly_LayoutVisible() {
        Runnable seeOtherTabsRunnable =
                mIncognitoReauthCoordinatorFactory.getSeeOtherTabsRunnable();
        if (mIsTabbedActivity) {
            when(mLayoutManagerMock.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
            doNothing().when(mTabModelSelectorMock).selectModel(/* incognito= */ false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));

            seeOtherTabsRunnable.run();

            verify(mLayoutManagerMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
            verify(mTabModelSelectorMock, times(1)).selectModel(/* incognito= */ eq(false));
            verify(mPaneManagerMock).focusPane(PaneId.TAB_SWITCHER);
        } else {
            doNothing().when(mContextMock).startActivity(mIntentMock);
            seeOtherTabsRunnable.run();
            verify(mContextMock, times(1)).startActivity(mIntentMock);
        }
    }

    @Test
    @SmallTest
    public void testCloseAllIncognitoTabsRunnable_IsInvokedCorrectly() {
        Runnable closeAllIncognitoTabsRunnable =
                mIncognitoReauthCoordinatorFactory.getCloseAllIncognitoTabsRunnable();
        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHostMock);
        closeAllIncognitoTabsRunnable.run();

        verify(mIncognitoTabHostMock, times(1)).closeAllIncognitoTabs();
    }

    @Test
    @SmallTest
    public void testBackPressRunnable_IsInvokedCorrectly_LayoutNotVisible() {
        Runnable backPressRunnable = mIncognitoReauthCoordinatorFactory.getBackPressRunnable();
        // Does the same thing as see other tabs runnable.
        if (mIsTabbedActivity) {
            when(mLayoutManagerMock.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
            doNothing().when(mTabModelSelectorMock).selectModel(/* incognito= */ false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));

            backPressRunnable.run();

            verify(mLayoutManagerMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
            verify(mTabModelSelectorMock, times(1)).selectModel(/* incognito= */ eq(false));
            verify(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));
        } else {
            doNothing().when(mContextMock).startActivity(mIntentMock);
            backPressRunnable.run();
            verify(mContextMock, times(1)).startActivity(mIntentMock);
        }
    }

    @Test
    @SmallTest
    public void testBackPressRunnable_IsInvokedCorrectly_LayoutVisible() {
        Runnable backPressRunnable = mIncognitoReauthCoordinatorFactory.getBackPressRunnable();
        // Does the same thing as see other tabs runnable.
        if (mIsTabbedActivity) {
            when(mLayoutManagerMock.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
            doNothing().when(mTabModelSelectorMock).selectModel(/* incognito= */ false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /* animate= */ eq(false));

            backPressRunnable.run();

            verify(mLayoutManagerMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
            verify(mTabModelSelectorMock, times(1)).selectModel(/* incognito= */ eq(false));
            verify(mPaneManagerMock).focusPane(PaneId.TAB_SWITCHER);
        } else {
            doNothing().when(mContextMock).startActivity(mIntentMock);
            backPressRunnable.run();
            verify(mContextMock, times(1)).startActivity(mIntentMock);
        }
    }

    @Test
    @SmallTest
    public void testCreateIncognitoReauthCoordinator_ForFullScreen_ReturnsFullScreenCoordinator() {
        if (mIsTabbedActivity) {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock,
                            /* showFullScreen= */ true,
                            mOnBackPressedCallbackMock);
            assertTrue(
                    "Wrong coordinator instance created!",
                    coordinator instanceof FullScreenIncognitoReauthCoordinator);
        } else {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock,
                            /* showFullScreen= */ true,
                            mOnBackPressedCallbackMock);
            assertTrue(
                    "Wrong coordinator instance created!",
                    coordinator instanceof FullScreenIncognitoReauthCoordinator);
        }
    }

    @Test
    @SmallTest
    public void
            testCreateIncognitoReauthCoordinator_ForTabSwitcher_ReturnsTabSwitcherCoordinator_ExceptForCCT() {
        if (mIsTabbedActivity) {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock,
                            /* showFullScreen= */ false,
                            mOnBackPressedCallbackMock);
            assertTrue(
                    "Wrong coordinator instance created!",
                    coordinator
                            .getClass()
                            .isAssignableFrom(TabSwitcherIncognitoReauthCoordinator.class));
        } else {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock,
                            /* showFullScreen= */ true,
                            mOnBackPressedCallbackMock);
            assertTrue(
                    "Wrong coordinator instance created!",
                    coordinator
                            .getClass()
                            .isAssignableFrom(FullScreenIncognitoReauthCoordinator.class));
        }
    }

    @Test
    @SmallTest
    public void testAreDependenciesReadyFor() {
        if (mIsTabbedActivity) {
            // The TabSwitcherCustomViewManager is set.
            assertTrue(
                    mIncognitoReauthCoordinatorFactory.areDependenciesReadyFor(
                            /* showFullScreen= */ true));
            assertTrue(
                    mIncognitoReauthCoordinatorFactory.areDependenciesReadyFor(
                            /* showFullScreen= */ false));
        } else {
            // The TabSwitcherCustomViewManager is not set.
            assertTrue(
                    mIncognitoReauthCoordinatorFactory.areDependenciesReadyFor(
                            /* showFullScreen= */ true));
            assertFalse(
                    mIncognitoReauthCoordinatorFactory.areDependenciesReadyFor(
                            /* showFullScreen= */ false));
        }
    }
}
