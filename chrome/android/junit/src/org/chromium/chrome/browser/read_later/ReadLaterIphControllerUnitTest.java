// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/** Unit test for {@link ReadLaterIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReadLaterIphControllerUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock AppMenuHandler mAppMenuHandler;
    @Mock UserEducationHelper mUserEducationHelper;
    @Captor ArgumentCaptor<IphCommand> mIphCommandCaptor;

    Activity mActivity;
    View mToolbarMenuButton;
    ReadLaterIphController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mToolbarMenuButton = new View(ContextUtils.getApplicationContext());

        mController =
                new ReadLaterIphController(
                        mActivity, mToolbarMenuButton, mAppMenuHandler, mUserEducationHelper);
    }

    @Test
    public void onCopyContextMenuItemClicked() {
        mController.onCopyContextMenuItemClicked();
        verify(mUserEducationHelper).requestShowIph(any());
    }

    @Test
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
