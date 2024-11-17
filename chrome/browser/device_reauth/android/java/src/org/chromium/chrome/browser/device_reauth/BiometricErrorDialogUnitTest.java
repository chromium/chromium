// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.res.Resources;
import android.text.SpannableString;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BiometricErrorDialogUnitTest {
    private FakeModalDialogManager mModalDialogManager =
            new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testShowsAndHidesLockoutErrorDialog() {
        BiometricErrorDialogController controller =
                new BiometricErrorDialogController(mActivity, mModalDialogManager);
        controller.showLockoutErrorDialog();
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(mDialogModel);

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testLockoutErrorDialogProperties() {
        BiometricErrorDialogController controller =
                new BiometricErrorDialogController(mActivity, mModalDialogManager);
        controller.showLockoutErrorDialog();
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        View dialogContentsView = mDialogModel.get(ModalDialogProperties.CUSTOM_VIEW);

        assertNotNull(dialogContentsView);

        Resources resources = mActivity.getResources();
        TextView title = dialogContentsView.findViewById(R.id.error_dialog_title);
        assertEquals(
                resources.getString(R.string.identity_check_lockout_error_title), title.getText());

        TextView description = dialogContentsView.findViewById(R.id.description);
        assertEquals(
                resources.getString(R.string.identity_check_lockout_error_description),
                description.getText());

        TextView moreDetails = dialogContentsView.findViewById(R.id.more_details);
        SpannableString expected =
                SpanApplier.applySpans(
                        resources.getString(R.string.identity_check_lockout_error_more_details),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(mActivity, (unusedView) -> {})));
        assertEquals(expected.toString(), moreDetails.getText().toString());
    }
}
