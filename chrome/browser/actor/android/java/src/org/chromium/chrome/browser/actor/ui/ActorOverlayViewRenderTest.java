// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
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
                    .setRevision(5)
                    .build();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BackPressHandlerRegistry mBackPressHandlerRegistry;
    @Mock private LayoutManager mLayoutManager;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    private TestBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    private TabObscuringHandler mTabObscuringHandler;
    private Activity mActivity;
    private ActorOverlayCoordinator mCoordinator;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private SettableMonotonicObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
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

                    mLayoutManagerSupplier = ObservableSuppliers.createMonotonic();
                    mLayoutManagerSupplier.set(mLayoutManager);
                    when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

                    mProfileSupplier = ObservableSuppliers.createMonotonic();
                    mProfileSupplier.set(mProfile);
                    ActorKeyedServiceFactory.setForTesting(mActorKeyedService);

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
                                    mSnackbarManager,
                                    mBackPressHandlerRegistry,
                                    mLayoutManagerSupplier,
                                    mProfileSupplier,
                                    mSideUiStateProvider);
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

        CriteriaHelper.pollUiThread(
                () -> {
                    return mParentView.getChildAt(0) != null
                            && mParentView.getChildAt(0).getWidth() > 0;
                },
                "View did not get layout dimensions");

        mRenderTestRule.render(mParentView, "actor_overlay_default");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActorOverlay_accountsForSideUi() throws Exception {
        ArgumentCaptor<SideUiObserver> sideUiObserverCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(sideUiObserverCaptor.capture());

        SideUiSpecs sideUiSpecs =
                new SideUiSpecs(/* leftContainerWidth= */ 50, /* rightContainerWidth= */ 250);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sideUiObserverCaptor.getValue().onSideUiSpecsChanged(sideUiSpecs);
                    mCoordinator.getMediator().setOverlayVisible(true);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return mParentView.getChildAt(0) != null
                            && mParentView.getChildAt(0).getWidth() > 0;
                },
                "View did not get layout dimensions");

        mRenderTestRule.render(mParentView, "actor_overlay_side_ui");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActorOverlayHovered() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.getMediator().setOverlayVisible(true);
                    mCoordinator.getView().setHovered(true);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return mParentView.getChildAt(0) != null
                            && mParentView.getChildAt(0).getWidth() > 0;
                },
                "View did not get layout dimensions");

        mRenderTestRule.render(mParentView, "actor_overlay_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActorOverlayWithTakeOverButton() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.getMediator().setOverlayVisible(true);
                    mCoordinator
                            .getModelForTesting()
                            .set(ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE, true);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return mParentView.getChildAt(0) != null
                            && mParentView.getChildAt(0).getWidth() > 0;
                },
                "View did not get layout dimensions");

        mRenderTestRule.render(mParentView, "actor_overlay_with_take_over_button");
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
        public boolean hasBottomControlsHeightAnimation() {
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
        public void hideAndroidControls(boolean animate) {}

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
