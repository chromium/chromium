// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link DocumentPictureInPictureHeaderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private DocumentPictureInPictureHeaderDelegate mDelegate;
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;

    private WebContents mOpenerWebContents;
    private WebContents mWebContents;
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
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelNatives);
        mOpenerWebContents =
                Mockito.mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        mWebContents =
                Mockito.mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        when(mOpenerWebContents.getVisibleUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
    }

    @Test
    public void testCreation() {
        mCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        mView,
                        mDesktopWindowStateManager,
                        mThemeColorProvider,
                        mActivity,
                        mDelegate,
                        /* isBackToTabShown= */ true,
                        mOpenerWebContents,
                        mWebContents);

        verify(mDesktopWindowStateManager).addObserver(any());
        verify(mThemeColorProvider).addThemeColorObserver(any());
        verify(mThemeColorProvider).addTintObserver(any());
        verify((WebContentsObserver.Observable) mOpenerWebContents).addObserver(any());
        verify((WebContentsObserver.Observable) mWebContents).addObserver(any());
    }

    @Test
    public void testDestroy() {
        mCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        mView,
                        mDesktopWindowStateManager,
                        mThemeColorProvider,
                        mActivity,
                        mDelegate,
                        /* isBackToTabShown= */ true,
                        mOpenerWebContents,
                        mWebContents);
        mCoordinator.destroy();

        verify(mDesktopWindowStateManager).removeObserver(any());
        verify(mThemeColorProvider).removeThemeColorObserver(any());
        verify(mThemeColorProvider).removeTintObserver(any());
        verify((WebContentsObserver.Observable) mOpenerWebContents).removeObserver(any());
        verify((WebContentsObserver.Observable) mWebContents).removeObserver(any());
    }

}
