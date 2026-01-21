// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

/** Unit tests for {@link DocumentPictureInPictureHeaderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private ThemeColorProvider mThemeColorProvider;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private View mView;
    private DocumentPictureInPictureHeaderCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();

        View mainView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.document_picture_in_picture_main_layout, null);
        mActivity.setContentView(mainView);
        mView = mainView.findViewById(R.id.document_picture_in_picture_header);
    }

    @Test
    public void testCreation() {
        mCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        mView, mDesktopWindowStateManager, mThemeColorProvider);

        verify(mDesktopWindowStateManager).addObserver(any());
    }

    @Test
    public void testDestroy() {
        mCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        mView, mDesktopWindowStateManager, mThemeColorProvider);
        mCoordinator.destroy();

        verify(mDesktopWindowStateManager).removeObserver(any());
        verify(mThemeColorProvider).removeThemeColorObserver(any());
    }
}
