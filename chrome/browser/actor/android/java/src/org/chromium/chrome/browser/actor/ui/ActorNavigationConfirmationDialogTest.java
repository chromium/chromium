// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ActorNavigationConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC)
public class ActorNavigationConfirmationDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Boolean> mCallback;
    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testShow_confirmLeave() {
        ActorNavigationConfirmationDialog.show(mActivity, mModalDialogManager, mCallback);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mPropertyModelCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.actor_leave_site_dialog_title),
                model.get(ModalDialogProperties.TITLE));

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Simulate positive button click (leave site)
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mCallback).onResult(true);
    }

    @Test
    public void testShow_cancel() {
        ActorNavigationConfirmationDialog.show(mActivity, mModalDialogManager, mCallback);

        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mPropertyModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Simulate negative button click (cancel/stay)
        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mCallback).onResult(false);
    }
}
