// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
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

import java.util.function.Supplier;

/** Tests for {@link ChromeTabModalPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private final MonotonicObservableSupplier<ScrimManager> mScrimManagerSupplier =
            ObservableSuppliers.alwaysNull();
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier =
            ObservableSuppliers.alwaysNull();

    private ToolbarManager mCurrentToolbarManager;
    private final UserDataHost mUserDataHost = new UserDataHost();

    private static class TestChromeTabModalPresenter extends ChromeTabModalPresenter {
        public TestChromeTabModalPresenter(
                Activity activity,
                Supplier<TabObscuringHandler> tabObscuringHandlerSupplier,
                Supplier<ToolbarManager> toolbarManagerSupplier,
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
                        () -> {
                            if (mCurrentToolbarManager == null) {
                                throw new AssertionError("Simulated RootUiCoordinator assertion");
                            }
                            return mCurrentToolbarManager;
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
        // This should NOT crash despite the AssertionError in the supplier, and should clear the
        // tab state.
        mPresenter.setBrowserControlsAccess(false);

        // Verify that the tab state IS cleared.
        assertFalse(
                "Dialog should NOT be showing on the tab after dismissal even if ToolbarManager"
                        + " access throws AssertionError",
                ChromeTabModalPresenter.isDialogShowing(mTab));
    }
}
