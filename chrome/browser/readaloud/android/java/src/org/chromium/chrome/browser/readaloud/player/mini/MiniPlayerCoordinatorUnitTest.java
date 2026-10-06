// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayerJni;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.browser.ui.side_ui.ViewMarginAdjusterForSideUi;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MiniPlayerCoordinatorUnitTest {
    private static final String TITLE = "Title";
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock ReadAloudMiniPlayerSceneLayer.Natives mSceneLayerNativeMock;

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private LayoutManager mLayoutManager;
    @Mock private MiniPlayerMediator mMediator;
    @Mock private ReadAloudMiniPlayerSceneLayer mSceneLayer;
    @Mock private PlayerCoordinator mPlayerCoordinator;
    @Mock private UserEducationHelper mUserEducationHelper;
    private Activity mActivity;
    private FrameLayout mContentView;
    private MiniPlayerLayout mLayout;
    private PropertyModel mSharedModel;
    private PropertyModel mModel;

    private MiniPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView = new FrameLayout(mActivity);
        mActivity.setContentView(mContentView);
        mLayout =
                (MiniPlayerLayout)
                        mActivity
                                .getLayoutInflater()
                                .inflate(
                                        R.layout.readaloud_mini_player_layout, mContentView, false);
        mContentView.addView(mLayout);
        mSharedModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mModel = new PropertyModel.Builder(Properties.ALL_KEYS).build();
        ReadAloudMiniPlayerSceneLayerJni.setInstanceForTesting(mSceneLayerNativeMock);
        doReturn(123456789L).when(mSceneLayerNativeMock).init(any());
        doReturn(mModel).when(mMediator).getModel();
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        mCoordinator =
                new MiniPlayerCoordinator(
                        mActivity,
                        mSharedModel,
                        mMediator,
                        mLayout,
                        mSceneLayer,
                        mLayoutManager,
                        mPlayerCoordinator,
                        mUserEducationHelper,
                        /* sideUiStateProviderSupplier= */ null);
    }

    @Test
    public void testViewInflated() {
        // Test the real constructor
        ViewStub viewStub = new ViewStub(mActivity, R.layout.readaloud_mini_player_layout);
        viewStub.setId(R.id.readaloud_mini_player_stub);
        mContentView.addView(viewStub);
        mCoordinator =
                new MiniPlayerCoordinator(
                        mActivity,
                        mActivity,
                        mSharedModel,
                        mBottomControlsStacker,
                        mLayoutManager,
                        mPlayerCoordinator,
                        mUserEducationHelper,
                        /* sideUiStateProviderSupplier= */ null);
        // The stub was replaced by the inflated layout.
        assertNull(viewStub.getParent());
        assertTrue(
                mContentView.getChildAt(mContentView.getChildCount() - 1)
                        instanceof MiniPlayerLayout);
        verify(mLayoutManager).addSceneOverlay(eq(mSceneLayer));
    }

    @Test
    public void testShow() {
        mCoordinator.show(/* animate= */ false);
        verify(mMediator).show(eq(false));

        mCoordinator.show(/* animate= */ false);
        verify(mMediator, times(2)).show(eq(false));
    }

    @Test
    public void testOnShown_requestingIph() {
        // If there's no container to anchor IPH against, don't request it.
        mCoordinator.onShown(/* iphAnchorView= */ null);
        verify(mUserEducationHelper, never()).requestShowIph(any(IphCommand.class));

        mCoordinator.onShown(new View(mActivity));
        verify(mUserEducationHelper).requestShowIph(any(IphCommand.class));
    }

    @Test
    public void testDismissWhenNeverShown() {
        // Ensure there's no crash.
        assertEquals(VisibilityState.GONE, mCoordinator.getVisibility());
        mCoordinator.dismiss(false);
    }

    @Test
    public void testDismiss() {
        mCoordinator.dismiss(/* animate= */ false);
        verify(mMediator).dismiss(eq(false));
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        assertEquals(
                mActivity.getString(R.string.readaloud_pause),
                mLayout.findViewById(R.id.play_button).getContentDescription());
    }

    @Test
    public void testBindTitle() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.TITLE, TITLE);
        assertEquals(TITLE, ((TextView) mLayout.findViewById(R.id.title)).getText().toString());
    }

    @Test
    public void testBindSubtitle() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PLAYBACK_MODE, PlaybackMode.OVERVIEW.getValue());
        assertEquals(
                mActivity.getString(R.string.readaloud_chrome_now_playing_audio_overview),
                ((TextView) mLayout.findViewById(R.id.subtitle)).getText().toString());
    }

    @Test
    public void testBindProgress() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PROGRESS, 0.5f);
        ProgressBar progressBar = mLayout.findViewById(R.id.progress_bar);
        assertEquals((int) (0.5f * progressBar.getMax()), progressBar.getProgress());
    }

    @Test
    public void testBindYOffset() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(Properties.Y_OFFSET, -100);
        assertEquals(100, ((MarginLayoutParams) mLayout.getLayoutParams()).bottomMargin);
    }

    @Test
    public void testSideUiStateProviderRegistration() {
        OneshotSupplierImpl<SideUiStateProvider> supplier = new OneshotSupplierImpl<>();
        SideUiStateProvider provider = Mockito.mock(SideUiStateProvider.class);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mLayout.getLayoutParams();

        mCoordinator =
                new MiniPlayerCoordinator(
                        mActivity,
                        mSharedModel,
                        mMediator,
                        mLayout,
                        mSceneLayer,
                        mLayoutManager,
                        mPlayerCoordinator,
                        mUserEducationHelper,
                        supplier);

        SideUiSpecs specs = new SideUiSpecs(10, 20);
        doReturn(specs).when(provider).getCurrentSideUiSpecs();

        // Before supplier is available, no observer is registered.
        verify(provider, never()).addObserver(any());

        // Set supplier
        supplier.set(provider);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Now, it should have registered an observer.
        verify(provider).addObserver(any(ViewMarginAdjusterForSideUi.class));

        // It should have applied the current specs immediately.
        assertEquals(10, layoutParams.leftMargin);
        assertEquals(20, layoutParams.rightMargin);

        // When coordinator is destroyed, it should remove the observer.
        mCoordinator.destroy();
        verify(provider).removeObserver(any(ViewMarginAdjusterForSideUi.class));
    }
}
