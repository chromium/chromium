// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

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

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Tests for {@link ChromeTabModalPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabModalPresenterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private TestChromeTabModalPresenter mPresenter;

    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TabObscuringHandler mTabObscuringHandler;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private WebContents mWebContents;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private ScrimManager mScrimManager;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private final SettableMonotonicObservableSupplier<ScrimManager> mScrimManagerSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();

    private ToolbarManager mCurrentToolbarManager;
    private final UserDataHost mUserDataHost = new UserDataHost();

    private static class TestChromeTabModalPresenter extends ChromeTabModalPresenter {
        public TestChromeTabModalPresenter(
                Activity activity,
                Supplier<TabObscuringHandler> tabObscuringHandlerSupplier,
                OneshotSupplier<ToolbarManager> toolbarManagerSupplier,
                Runnable hideContextualSearch,
                FullscreenManager fullscreenManager,
                BrowserControlsVisibilityManager browserControlsVisibilityManager,
                TabModelSelector tabModelSelector,
                MonotonicObservableSupplier<ScrimManager> scrimManagerSupplier,
                MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
            super(
                    activity,
                    tabObscuringHandlerSupplier,
                    toolbarManagerSupplier,
                    hideContextualSearch,
                    fullscreenManager,
                    browserControlsVisibilityManager,
                    tabModelSelector,
                    scrimManagerSupplier,
                    edgeToEdgeControllerSupplier);
        }

        @Override
        public void setBrowserControlsAccess(boolean restricted) {
            super.setBrowserControlsAccess(restricted);
        }

        @Override
        public void showDialogContainer() {
            super.showDialogContainer();
        }

        @Override
        protected void runEnterAnimation() {}

        // Calls are verified on the spy; the real method needs a SelectionPopupController.
        @Override
        protected void saveOrRestoreTextSelection(
                WebContents webContents, boolean save, boolean restoreFocus) {}

        private ViewGroup mTestDialogContainer;

        public void setDialogContainerForTesting(ViewGroup container) {
            mTestDialogContainer = container;
        }

        @Override
        protected ViewGroup getDialogContainer() {
            return mTestDialogContainer != null ? mTestDialogContainer : super.getDialogContainer();
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mCurrentToolbarManager = mToolbarManager;

        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        mPresenter =
                new TestChromeTabModalPresenter(
                        mActivity,
                        () -> mTabObscuringHandler,
                        new OneshotSupplier<ToolbarManager>() {
                            @Override
                            public ToolbarManager onAvailable(
                                    org.chromium.base.Callback<ToolbarManager> callback) {
                                return get();
                            }

                            @Override
                            public ToolbarManager get() {
                                return mCurrentToolbarManager;
                            }
                        },
                        () -> {},
                        mFullscreenManager,
                        mBrowserControlsVisibilityManager,
                        mTabModelSelector,
                        mScrimManagerSupplier,
                        mEdgeToEdgeControllerSupplier);
    }

    @Test
    public void testDismissAfterToolbarManagerDestroyed() {
        // Set mActiveTab in the presenter.
        mPresenter.setActiveTabForTesting(mTab);

        // Manually set the attribute.
        TabAttributes.from(mTab).set(TabAttributeKeys.MODAL_DIALOG_SHOWING, true);
        assertTrue(
                "Dialog should be showing on the tab",
                ChromeTabModalPresenter.isDialogShowing(mTab));

        // Simulate activity destruction order where ToolbarManager is nulled out before
        // ModalDialogManager.
        mCurrentToolbarManager = null;

        // Dismiss the dialog (this calls setBrowserControlsAccess(false)).
        // This should NOT crash despite the ToolbarManager being destroyed, and should clear the
        // tab state.
        mPresenter.setBrowserControlsAccess(false);

        // Verify that the tab state IS cleared.
        assertFalse(
                "Dialog should NOT be showing on the tab after dismissal even if ToolbarManager"
                        + " is destroyed",
                ChromeTabModalPresenter.isDialogShowing(mTab));
    }

    @Test
    public void testDismissAfterTabDestroyed() {
        // Set the active tab in the presenter.
        mPresenter.setActiveTabForTesting(mTab);

        // Simulate that a dialog is currently showing on the tab.
        TabAttributes.from(mTab).set(TabAttributeKeys.MODAL_DIALOG_SHOWING, true);

        // Mock the tab being destroyed.
        when(mTab.isDestroyed()).thenReturn(true);

        // Simulate dismissing the dialog. This should not crash even though the tab is destroyed.
        mPresenter.setBrowserControlsAccess(false);
    }

    /**
     * Simulates a dialog shown over {@link #mTab} being hidden, by calling
     * setBrowserControlsAccess(false) with the given tab state.
     */
    private void simulateDialogHidden(boolean tabInteractable, Tab currentTab) {
        mPresenter = spy(mPresenter);
        mPresenter.setActiveTabForTesting(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isUserInteractable()).thenReturn(tabInteractable);
        when(mTabModelSelector.getCurrentTab()).thenReturn(currentTab);
        mPresenter.setBrowserControlsAccess(false);
    }

    @Test
    public void testSetBrowserControlsAccess_TabInteractableAndSelected_RestoresFocus() {
        simulateDialogHidden(/* tabInteractable= */ true, mTab);
        verify(mPresenter)
                .saveOrRestoreTextSelection(
                        mWebContents, /* save= */ false, /* restoreFocus= */ true);
    }

    @Test
    public void testSetBrowserControlsAccess_TabNotInteractable_DoesNotRestoreFocus() {
        // E.g. the dialog is suspended because the tab switcher is showing.
        simulateDialogHidden(/* tabInteractable= */ false, mTab);
        verify(mPresenter)
                .saveOrRestoreTextSelection(
                        mWebContents, /* save= */ false, /* restoreFocus= */ false);
        verify(mPresenter, never())
                .saveOrRestoreTextSelection(any(), anyBoolean(), eq(/* restoreFocus= */ true));
    }

    @Test
    public void testSetBrowserControlsAccess_TabNotSelected_DoesNotRestoreFocus() {
        // E.g. another tab was selected while the dialog was showing.
        simulateDialogHidden(/* tabInteractable= */ true, mock(Tab.class));
        verify(mPresenter)
                .saveOrRestoreTextSelection(
                        mWebContents, /* save= */ false, /* restoreFocus= */ false);
        verify(mPresenter, never())
                .saveOrRestoreTextSelection(any(), anyBoolean(), eq(/* restoreFocus= */ true));
    }

    @Test
    public void testTopControlsHeightChanged_UpdatesScrimTopMargin() {
        ViewGroup container = new FrameLayout(mActivity);
        container.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mPresenter.setDialogContainerForTesting(container);

        PropertyModel scrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 56)
                        .build();
        mPresenter.setScrimModelForTesting(scrimModel);

        when(mBrowserControlsVisibilityManager.getTopControlsHeight()).thenReturn(96);

        mPresenter.onTopControlsHeightChanged(96, 0);

        assertEquals(
                "Dialog container top margin should be updated.",
                96,
                ((MarginLayoutParams) container.getLayoutParams()).topMargin);
        assertEquals(
                "Scrim top margin should be updated.",
                96,
                scrimModel.get(ScrimProperties.TOP_MARGIN));
        assertFalse(
                "Scrim should not affect status bar when top edge-to-edge is inactive.",
                scrimModel.get(ScrimProperties.AFFECTS_STATUS_BAR));
    }

    @Test
    public void testShowDialogContainer_TopEdgeToEdgeWithOnlyScalp_ScrimAffectsStatusBar() {
        mScrimManagerSupplier.set(mScrimManager);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mBrowserControlsVisibilityManager.getTopControlsHeight()).thenReturn(60);

        ViewGroup container = new FrameLayout(mActivity);
        container.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mPresenter.setDialogContainerForTesting(container);

        mPresenter.onTopControlsHeightChanged(60, 0);
        mPresenter.showDialogContainer();

        assertEquals(
                "Dialog container top margin should stay below status bar.",
                60,
                ((MarginLayoutParams) container.getLayoutParams()).topMargin);
        PropertyModel scrimModel = mPresenter.getScrimModelForTesting();
        assertTrue(
                "Scrim should affect status bar when only top scalp is at the top.",
                scrimModel.get(ScrimProperties.AFFECTS_STATUS_BAR));
        assertEquals(
                "Scrim top margin should be 0 when covering the status bar.",
                0,
                scrimModel.get(ScrimProperties.TOP_MARGIN));
    }

    @Test
    public void testTopControlsHeightChanged_TopEdgeToEdgeWithTopToolbar_DoesNotAffectStatusBar() {
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mBrowserControlsVisibilityManager.getTopControlsHeight()).thenReturn(156);

        ViewGroup container = new FrameLayout(mActivity);
        container.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mPresenter.setDialogContainerForTesting(container);

        PropertyModel scrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .build();
        mPresenter.setScrimModelForTesting(scrimModel);

        mPresenter.onTopControlsHeightChanged(156, 0);

        assertEquals(
                "Dialog container top margin should equal top controls height.",
                156,
                ((MarginLayoutParams) container.getLayoutParams()).topMargin);
        assertFalse(
                "Scrim should not affect status bar when top toolbar is visible.",
                scrimModel.get(ScrimProperties.AFFECTS_STATUS_BAR));
        assertEquals(
                "Scrim top margin should equal top controls height.",
                156,
                scrimModel.get(ScrimProperties.TOP_MARGIN));
    }

    @Test
    public void
            testTopControlsHeightChanged_ZeroTopControlsWithTopInset_ClampsContainerTopMargin() {
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        when(mBrowserControlsVisibilityManager.getTopControlsHeight()).thenReturn(0);

        ViewGroup container = new FrameLayout(mActivity);
        container.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        mPresenter.setDialogContainerForTesting(container);

        PropertyModel scrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 60)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                        .build();
        mPresenter.setScrimModelForTesting(scrimModel);

        mPadAdjusterCaptor.getValue().overrideTopInset(60);

        assertEquals(
                "Dialog container top margin should be clamped to topInset.",
                60,
                ((MarginLayoutParams) container.getLayoutParams()).topMargin);
        assertTrue(
                "Scrim should affect status bar when top controls height is 0.",
                scrimModel.get(ScrimProperties.AFFECTS_STATUS_BAR));
        assertEquals(
                "Scrim top margin should be 0 when covering the status bar.",
                0,
                scrimModel.get(ScrimProperties.TOP_MARGIN));

        mPresenter.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(mPadAdjusterCaptor.getValue());
    }
}
