// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.actor.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link ActorOverlayView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ActorOverlayViewRenderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(1)
                    .build();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SnackbarManager mSnackbarManager;
    private TestBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    private TabObscuringHandler mTabObscuringHandler;
    private Activity mActivity;
    private ActorOverlayCoordinator mCoordinator;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private FrameLayout mParentView;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        mBrowserControlsVisibilityManager = new TestBrowserControlsVisibilityManager();
        mBrowserControlsVisibilityManager.topControlsHeight = 100;
        mBrowserControlsVisibilityManager.bottomControlsHeight = 100;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabObscuringHandler = new TabObscuringHandler();
                    mCurrentTabSupplier = ObservableSuppliers.createNullable();
                    when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);

                    mParentView = new FrameLayout(mActivity);
                    mActivity.setContentView(mParentView);

                    ViewStub viewStub = new ViewStub(mActivity);
                    viewStub.setLayoutResource(R.layout.actor_overlay);
                    mParentView.addView(viewStub);

                    mCoordinator =
                            new ActorOverlayCoordinator(
                                    viewStub,
                                    mTabModelSelector,
                                    mBrowserControlsVisibilityManager,
                                    mTabObscuringHandler,
                                    mSnackbarManager);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActorOverlay() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.getMediator().setOverlayVisible(true);
                });
        mRenderTestRule.render(mParentView, "actor_overlay_default");
    }

    // Test implementation that used to bypass mockito limitations on mocking extended interfaces.
    private static class TestBrowserControlsVisibilityManager
            implements BrowserControlsVisibilityManager {
        public int topControlsHeight;
        public int bottomControlsHeight;

        @Override
        public void addObserver(BrowserControlsStateProvider.Observer obs) {}

        @Override
        public void removeObserver(BrowserControlsStateProvider.Observer obs) {}

        @Override
        public int getTopControlsHeight() {
            return topControlsHeight;
        }

        @Override
        public int getTopControlsHairlineHeight() {
            return 0;
        }

        @Override
        public int getTopControlsMinHeight() {
            return 0;
        }

        @Override
        public int getTopControlOffset() {
            return 0;
        }

        @Override
        public int getTopControlsMinHeightOffset() {
            return 0;
        }

        @Override
        public int getBottomControlsHeight() {
            return bottomControlsHeight;
        }

        @Override
        public int getBottomControlsMinHeight() {
            return 0;
        }

        @Override
        public int getBottomControlsMinHeightOffset() {
            return 0;
        }

        @Override
        public boolean shouldAnimateBrowserControlsHeightChanges() {
            return false;
        }

        @Override
        public int getBottomControlOffset() {
            return 0;
        }

        @Override
        public float getBrowserControlHiddenRatio() {
            return 0;
        }

        @Override
        public int getContentOffset() {
            return 0;
        }

        @Override
        public float getTopVisibleContentOffset() {
            return 0;
        }

        @Override
        public int getAndroidControlsVisibility() {
            return 0;
        }

        @Override
        public int getControlsPosition() {
            return BrowserControlsStateProvider.ControlsPosition.TOP;
        }

        @Override
        public boolean isVisibilityForced() {
            return false;
        }

        @Override
        public BrowserStateBrowserControlsVisibilityDelegate getBrowserVisibilityDelegate() {
            return null;
        }

        @Override
        public void showAndroidControls(boolean animate) {}

        @Override
        public void restoreControlsPositions() {}

        @Override
        public boolean offsetOverridden() {
            return false;
        }

        @Override
        public int hideAndroidControlsAndClearOldToken(int oldToken) {
            return 0;
        }

        @Override
        public void releaseAndroidControlsHidingToken(int token) {}
    }
}
