// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.PlayerState;
import org.chromium.chrome.browser.readaloud.R;
import org.chromium.chrome.browser.readaloud.miniplayer.MiniPlayerCoordinator.Observer;

/** Unit tests for {@link MiniPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerCoordinatorUnitTest {
    @Mock
    private ViewStub mViewStub;
    @Mock
    private LinearLayout mView;
    @Mock
    private ImageView mCloseButton;
    @Mock
    private TextView mTitleView;
    @Mock
    private TextView mPublisherView;
    @Mock
    private LinearLayout mTitleAndPublisherView;
    @Mock
    private Observer mObserver;

    @Captor
    private ArgumentCaptor<View.OnClickListener> mCloseCaptor;
    @Captor
    private ArgumentCaptor<View.OnClickListener> mExpandCaptor;

    private MiniPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mView).when(mViewStub).inflate();
        doReturn(mCloseButton)
                .when(mView)
                .findViewById(eq(R.id.readaloud_mini_player_close_button));
        doReturn(mTitleAndPublisherView)
                .when(mView)
                .findViewById(eq(R.id.readaloud_mini_player_title_and_publisher));
        doReturn(mTitleView).when(mView).findViewById(eq(R.id.readaloud_mini_player_title));
        doReturn(mPublisherView).when(mView).findViewById(eq(R.id.readaloud_mini_player_publisher));
        mCoordinator = new MiniPlayerCoordinator(mViewStub);
    }

    @Test
    public void testShowInflatesViewOnce() {
        mCoordinator.show(/*animate=*/false, /*playback=*/null);
        verify(mViewStub, times(1)).inflate();

        // Second show() shouldn't inflate the stub again.
        reset(mViewStub);
        mCoordinator.show(/*animate=*/false, /*playback=*/null);
        verify(mViewStub, never()).inflate();
    }

    @Test
    public void testObserveClose() {
        mCoordinator.addObserver(mObserver);
        mCoordinator.show(/*animate=*/false, /*playback=*/null);

        verify(mCloseButton).setOnClickListener(mCloseCaptor.capture());

        mCloseCaptor.getValue().onClick(mCloseButton);

        verify(mObserver, times(1)).onCloseClicked();
    }

    @Test
    public void testObserveExpand() {
        mCoordinator.addObserver(mObserver);
        mCoordinator.show(/*animate=*/false, /*playback=*/null);

        verify(mTitleAndPublisherView).setOnClickListener(mExpandCaptor.capture());

        mExpandCaptor.getValue().onClick(mTitleAndPublisherView);

        verify(mObserver, times(1)).onExpandRequested();
    }

    @Test
    public void testDismissWhenNeverShown() {
        // Check that methods depending on the mediator don't crash when it's null.
        assertEquals(PlayerState.GONE, mCoordinator.getState());
        mCoordinator.dismiss(false);
    }

    @Test
    public void testShowDismiss() {
        mCoordinator.show(/*animate=*/false, /*playback=*/null);
        mCoordinator.dismiss(/*animate=*/false);
        assertEquals(PlayerState.GONE, mCoordinator.getState());
    }
}
