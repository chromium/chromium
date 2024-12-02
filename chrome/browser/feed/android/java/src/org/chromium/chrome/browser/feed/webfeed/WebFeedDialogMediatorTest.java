// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static android.os.Looper.getMainLooper;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;

import androidx.activity.ComponentDialog;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Test for the WebFeedDialogMediatorTest class. */
@RunWith(BaseRobolectricTestRunner.class)
public final class WebFeedDialogMediatorTest {
    @Mock private View mView;
    @Mock private Callback<Integer> mButtonCallback;

    private WebFeedDialogMediator mMediator;
    private ModalDialogManager mModalDialogManager;
    private Activity mActivity;

    private static class Presenter extends ModalDialogManager.Presenter {
        @Override
        protected void addDialogView(
                PropertyModel model, @Nullable Callback<ComponentDialog> onDialogShownCallback) {}

        @Override
        protected void removeDialogView(PropertyModel model) {}
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.setupActivity(Activity.class);
        mModalDialogManager =
                new ModalDialogManager(new Presenter(), ModalDialogManager.ModalDialogType.APP);
        mMediator = new WebFeedDialogMediator(mModalDialogManager);

        mMediator.initialize(
                mView,
                new WebFeedDialogContents(
                        "title",
                        "details",
                        /* illustrationId= */ 2,
                        "primary button",
                        "secondary button",
                        mButtonCallback));
    }

    @Test
    @SmallTest
    public void showAndClickPositive_callsCallbackOnce() {
        mMediator.showDialog();
        mModalDialogManager
                .getCurrentDialogForTest()
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(null, ModalDialogProperties.ButtonType.POSITIVE);

        shadowOf(getMainLooper()).idle();

        verify(mButtonCallback, times(1)).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void showAndClickNegative_callsCallbackOnce() {
        mMediator.showDialog();
        mModalDialogManager
                .getCurrentDialogForTest()
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(null, ModalDialogProperties.ButtonType.NEGATIVE);

        shadowOf(getMainLooper()).idle();

        verify(mButtonCallback, times(1)).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void showAndDismissWithoutClick_callsCallbackOnce() {
        mMediator.showDialog();
        mModalDialogManager.dismissAllDialogs(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        shadowOf(getMainLooper()).idle();

        verify(mButtonCallback, times(1)).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
