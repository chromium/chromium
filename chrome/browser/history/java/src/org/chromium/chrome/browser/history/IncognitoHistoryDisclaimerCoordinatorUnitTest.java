// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Looper;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link IncognitoHistoryDisclaimerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoHistoryDisclaimerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid mOtherWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabRemover mIncognitoTabRemover;
    @Mock private TabRemover mRegularTabRemover;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private final UnownedUserDataHost mOtherUserDataHost = new UnownedUserDataHost();
    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private IncognitoHistoryDisclaimerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class);
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivityController.setup();
        View anchor = new View(mActivity);
        anchor.setId(View.generateViewId());
        FrameLayout content = new FrameLayout(mActivity);
        content.addView(anchor, new FrameLayout.LayoutParams(100, 100));
        mActivity.setContentView(content);

        doReturn(mUserDataHost).when(mWindowAndroid).getUnownedUserDataHost();
        doReturn(mOtherUserDataHost).when(mOtherWindowAndroid).getUnownedUserDataHost();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(/* incognito= */ true);
        doReturn(mIncognitoTabRemover).when(mIncognitoTabModel).getTabRemover();
        doReturn(mRegularTabModel).when(mTabModelSelector).getModel(/* incognito= */ false);
        doReturn(mRegularTabRemover).when(mRegularTabModel).getTabRemover();
        doReturn(true).when(mIncognitoTabModel).isIncognitoBranded();
        doReturn(false).when(mRegularTabModel).isIncognitoBranded();
        mCurrentTabModelSupplier.set(mIncognitoTabModel);
        doReturn(mCurrentTabModelSupplier).when(mTabModelSelector).getCurrentTabModelSupplier();
        doReturn(true).when(mTabModelSelector).isIncognitoBrandedModelSelected();
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        mCoordinator =
                new IncognitoHistoryDisclaimerCoordinator(
                        mActivity, mWindowAndroid, new int[] {anchor.getId()});
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        mActivityController.destroy();
    }

    private View showAndGetContent() {
        IncognitoHistoryDisclaimerCoordinator.showForWindow(mWindowAndroid);
        assertTrue(mCoordinator.isShowing());
        View content = mCoordinator.getContentViewForTesting();
        assertNotNull(content);
        return content;
    }

    @Test
    public void testGotIt_dismissesWithoutClosingTabs() {
        View content = showAndGetContent();

        content.findViewById(R.id.history_disclaimer_got_it).performClick();

        assertFalse(mCoordinator.isShowing());
        verify(mIncognitoTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    @Test
    public void testCloseIncognito_dismissesThenClosesThisWindowsIncognitoTabs() {
        View content = showAndGetContent();
        doAnswer(
                        invocation -> {
                            assertFalse(mCoordinator.isShowing());
                            return null;
                        })
                .when(mIncognitoTabRemover)
                .closeTabs(any(), anyBoolean());

        content.findViewById(R.id.history_disclaimer_close).performClick();

        verify(mIncognitoTabRemover)
                .closeTabs(
                        argThat((TabClosureParams params) -> params.isAllTabs),
                        /* allowDialog= */ eq(false));
        // The regular model is stubbed, so closing it would reach these asserts instead of
        // failing on a null mock.
        verify(mTabModelSelector, never()).getModel(/* incognito= */ false);
        verify(mRegularTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    @Test
    public void testShowWhileShowing_isNoOp() {
        View content = showAndGetContent();

        mCoordinator.show();

        assertTrue(mCoordinator.isShowing());
        assertEquals(content, mCoordinator.getContentViewForTesting());
    }

    @Test
    public void testSwitchToRegularModel_dismisses() {
        showAndGetContent();

        mCurrentTabModelSupplier.set(mRegularTabModel);

        assertFalse(mCoordinator.isShowing());
    }

    @Test
    public void testDestroy_dismissesAndUnregisters() {
        showAndGetContent();

        mUserDataHost.destroy();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertFalse(mCoordinator.isShowing());
        IncognitoHistoryDisclaimerCoordinator.showForWindow(mWindowAndroid);
        assertFalse(mCoordinator.isShowing());
        assertNull(mCoordinator.getContentViewForTesting());
    }

    @Test
    public void testPopupMargin_subtractsBackgroundInset() {
        // Stands in for menu_bg_tinted's shadow inset, which Robolectric doesn't load.
        Drawable background = new InsetDrawable(new ColorDrawable(), 11, 0, 11, 0);

        assertEquals(21, IncognitoHistoryDisclaimerCoordinator.getPopupMarginPx(background, 32));
        assertEquals(0, IncognitoHistoryDisclaimerCoordinator.getPopupMarginPx(background, 8));
    }

    @Test
    public void testShow_regularModelSelected_doesNotShow() {
        doReturn(false).when(mTabModelSelector).isIncognitoBrandedModelSelected();

        mCoordinator.show();

        assertFalse(mCoordinator.isShowing());
    }

    /** Creates a coordinator for a new window whose activity is created but not laid out yet. */
    private IncognitoHistoryDisclaimerCoordinator createBeforeLayout(
            ActivityController<TestActivity> controller) {
        Activity activity = controller.get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        controller.create();
        activity.setContentView(new FrameLayout(activity));
        assertFalse(activity.getWindow().getDecorView().isLaidOut());
        return new IncognitoHistoryDisclaimerCoordinator(
                activity, mOtherWindowAndroid, new int[] {View.generateViewId()});
    }

    private void layOut(ActivityController<TestActivity> controller) {
        controller.start().resume().visible();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testShowBeforeLayout_showsOnceAfterLayout() {
        ActivityController<TestActivity> controller = Robolectric.buildActivity(TestActivity.class);
        IncognitoHistoryDisclaimerCoordinator coordinator = createBeforeLayout(controller);

        IncognitoHistoryDisclaimerCoordinator destroyedCoordinator =
                new IncognitoHistoryDisclaimerCoordinator(
                        controller.get(), mWindowAndroid, new int[] {View.generateViewId()});
        destroyedCoordinator.show();
        assertTrue(destroyedCoordinator.hasPendingShowForTesting());
        destroyedCoordinator.destroy();
        assertFalse(destroyedCoordinator.hasPendingShowForTesting());

        coordinator.show();
        coordinator.show();
        assertFalse(coordinator.isShowing());

        layOut(controller);
        assertTrue(coordinator.isShowing());
        assertFalse(destroyedCoordinator.isShowing());
        View content = coordinator.getContentViewForTesting();

        // Later layouts don't re-show or rebuild the popup.
        ViewUtils.requestLayout(
                controller.get().getWindow().getDecorView(),
                "IncognitoHistoryDisclaimerCoordinatorUnitTest.testShowBeforeLayout");
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertEquals(content, coordinator.getContentViewForTesting());

        // The one-shot layout wait is gone, so a layout after dismissal doesn't bring it back.
        content.findViewById(R.id.history_disclaimer_got_it).performClick();
        assertFalse(coordinator.isShowing());
        ViewUtils.requestLayout(
                controller.get().getWindow().getDecorView(),
                "IncognitoHistoryDisclaimerCoordinatorUnitTest.testShowBeforeLayout");
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertFalse(coordinator.isShowing());

        controller.get().finish();
        coordinator.show();
        assertFalse(coordinator.isShowing());

        coordinator.destroy();
        controller.destroy();
    }

    @Test
    public void testShowForWindow_withoutCoordinator_doesNothing() {
        IncognitoHistoryDisclaimerCoordinator.showForWindow(mOtherWindowAndroid);
        IncognitoHistoryDisclaimerCoordinator.showForWindow(null);

        assertNull(
                IncognitoHistoryDisclaimerCoordinator.getForWindowForTesting(mOtherWindowAndroid));
        verify(mTabModelSelector, never()).isIncognitoBrandedModelSelected();
    }
}
