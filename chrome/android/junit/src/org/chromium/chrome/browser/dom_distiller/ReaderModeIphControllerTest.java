// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/** Unit tests for {@link ReaderModeIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeIphControllerTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mAnchorView;
    @Mock private AppMenuHandler mAppMenuHandler;

    private ReaderModeIphController mController;

    @Before
    public void setUp() {
        mController =
                new ReaderModeIphController(mUserEducationHelper, mAnchorView, mAppMenuHandler);
    }

    @Test
    public void testShowIph() {
        mController.showIph();

        ArgumentCaptor<IphCommand> captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());

        IphCommand command = captor.getValue();
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.reader_mode_menu_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
