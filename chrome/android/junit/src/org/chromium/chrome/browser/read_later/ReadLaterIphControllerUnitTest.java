// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/** Unit test for {@link ReadLaterIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadLaterIphControllerUnitTest {

    @Mock Activity mActivity;
    @Mock View mToolbarMenuButton;
    @Mock AppMenuHandler mAppMenuHandler;
    @Mock UserEducationHelper mUserEducationHelper;
    @Mock Context mContext;
    @Mock Resources mResources;
    @Captor ArgumentCaptor<IphCommand> mIphCommandCaptor;

    ReadLaterIphController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        mController =
                new ReadLaterIphController(
                        mActivity, mToolbarMenuButton, mAppMenuHandler, mUserEducationHelper);
    }

    @Test
    @SmallTest
    public void onCopyContextMenuItemClicked() {
        mController.onCopyContextMenuItemClicked();
        verify(mUserEducationHelper).requestShowIph(any());
    }

    @Test
    @SmallTest
    public void showColdStartIph() {
        mController.showColdStartIph();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.all_bookmarks_menu_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
