// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.content.Context;
import android.content.Intent;

import androidx.activity.OnBackPressedCallback;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Arrays;
import java.util.Collection;

/**
 * Robolectric tests for {@link IncognitoReauthCoordinatorFactory}.
 *
 * TODO(crbug.com/1227656): Remove parameterization to improve readability of the tests.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(UNIT_TESTS)
public class IncognitoReauthCoordinatorFactoryTest {
    @Mock
    private Context mContextMock;
    @Mock
    private TabModelSelector mTabModelSelectorMock;
    @Mock
    private ModalDialogManager mModalDialogManagerMock;
    @Mock
    private IncognitoReauthManager mIncognitoReauthManagerMock;
    @Mock
    private SettingsLauncher mSettingsLauncherMock;
    @Mock
    private TabSwitcherCustomViewManager mTabSwitcherCustomViewManagerMock;
    @Mock
    private IncognitoReauthTopToolbarDelegate mIncognitoReauthTopToolbarDelegateMock;
    @Mock
    private LayoutManager mLayoutManagerMock;
    @Mock
    private Intent mIntentMock;
    @Mock
    private TabModel mIncognitoTabModelMock;
    @Mock
    private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock
    private IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegateMock;
    @Mock
    private IncognitoTabHost mIncognitoTabHostMock;

    private OnBackPressedCallback mOnBackPressedCallbackMock = new OnBackPressedCallback(false) {
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
        mIncognitoReauthCoordinatorFactory = new IncognitoReauthCoordinatorFactory(mContextMock,
                mTabModelSelectorMock, mModalDialogManagerMock, mIncognitoReauthManagerMock,
                mSettingsLauncherMock, mIncognitoReauthTopToolbarDelegateMock, mLayoutManagerMock,
                mIntentMock, mIsTabbedActivity);

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

        verifyNoMoreInteractions(mContextMock, mTabModelSelectorMock, mIncognitoTabModelMock,
                mModalDialogManagerMock, mIncognitoReauthManagerMock, mSettingsLauncherMock,
                mTabSwitcherCustomViewManagerMock, mIncognitoReauthTopToolbarDelegateMock,
                mLayoutManagerMock, mIncognitoTabHostMock);
    }

    @Test
    @SmallTest
    public void testSeeOtherTabsRunnable_IsInvokedCorrectly() {
        Runnable seeOtherTabsRunnable =
                mIncognitoReauthCoordinatorFactory.getSeeOtherTabsRunnable();
        if (mIsTabbedActivity) {
            doNothing().when(mTabModelSelectorMock).selectModel(/*incognito=*/false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /*animate= */ eq(false));

            seeOtherTabsRunnable.run();

            verify(mTabModelSelectorMock, times(1)).selectModel(/*incognito=*/eq(false));
            verify(mLayoutManagerMock, times(1))
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /*animate= */ eq(false));
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
    public void testBackPressRunnable_IsInvokedCorrectly() {
        Runnable backPressRunnable = mIncognitoReauthCoordinatorFactory.getBackPressRunnable();
        // Does the same thing as see other tabs runnable.
        if (mIsTabbedActivity) {
            doNothing().when(mTabModelSelectorMock).selectModel(/*incognito=*/false);
            doNothing()
                    .when(mLayoutManagerMock)
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /*animate= */ eq(false));

            backPressRunnable.run();

            verify(mTabModelSelectorMock, times(1)).selectModel(/*incognito=*/eq(false));
            verify(mLayoutManagerMock, times(1))
                    .showLayout(eq(LayoutType.TAB_SWITCHER), /*animate= */ eq(false));
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
                            mIncognitoReauthCallbackMock, /*showFullScreen=*/true,
                            mOnBackPressedCallbackMock);
            assertTrue("Wrong coordinator instance created!",
                    coordinator instanceof FullScreenIncognitoReauthCoordinator);
        } else {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock, /*showFullScreen=*/true,
                            mOnBackPressedCallbackMock);
            assertTrue("Wrong coordinator instance created!",
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
                            mIncognitoReauthCallbackMock, /*showFullScreen=*/false,
                            mOnBackPressedCallbackMock);
            assertTrue("Wrong coordinator instance created!",
                    coordinator.getClass().isAssignableFrom(
                            TabSwitcherIncognitoReauthCoordinator.class));
        } else {
            IncognitoReauthCoordinator coordinator =
                    mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                            mIncognitoReauthCallbackMock, /*showFullScreen=*/true,
                            mOnBackPressedCallbackMock);
            assertTrue("Wrong coordinator instance created!",
                    coordinator.getClass().isAssignableFrom(
                            FullScreenIncognitoReauthCoordinator.class));
        }
    }
}
