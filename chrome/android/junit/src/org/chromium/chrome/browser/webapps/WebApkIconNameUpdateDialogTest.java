// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Test for the dialog warning that WebApks have an updated name/icon. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkIconNameUpdateDialogTest {
    // A callback that fires when an action is taken in a dialog.
    private final CallbackHelper mOnActionCallback = new CallbackHelper();
    private final MockModalDialogManager mDialogManager = new MockModalDialogManager();

    // The length of the explanation header when icon updates are requested.
    private static final int MESSAGE_HEADER_LENGTH = 88;

    private Integer mLastDismissalCause;

    private static class DialogParams {
        public static DialogParams createDefault() {
            DialogParams dialogParams = new DialogParams();
            dialogParams.iconChanged = false;
            dialogParams.expectIconShownAnyway = false;
            dialogParams.bitmapBefore = null;
            dialogParams.bitmapAfter = null;
            dialogParams.shortNameChanged = false;
            dialogParams.expectShortNameShownAnyway = false;
            dialogParams.shortNameBefore = "";
            dialogParams.shortNameAfter = "";
            dialogParams.nameChanged = false;
            dialogParams.expectNameHiddenAnyway = false;
            dialogParams.nameBefore = "";
            dialogParams.nameAfter = "";
            dialogParams.hasExplanationString = true;
            return dialogParams;
        }

        public boolean iconChanged;
        public boolean expectIconShownAnyway;
        public Bitmap bitmapBefore;
        public Bitmap bitmapAfter;
        public boolean shortNameChanged;
        public boolean expectShortNameShownAnyway;
        public String shortNameBefore;
        public String shortNameAfter;
        public boolean nameChanged;
        public boolean expectNameHiddenAnyway;
        public String nameBefore;
        public String nameAfter;
        public boolean hasExplanationString;
    }

    private static class MockModalDialogManager extends ModalDialogManager {
        private PropertyModel mCurrentDialogModel;

        public MockModalDialogManager() {
            super(Mockito.mock(Presenter.class), 0);
        }

        @Override
        public void showDialog(PropertyModel model, int dialogType) {
            mCurrentDialogModel = model;
        }

        public PropertyModel getCurrentDialogModel() {
            return mCurrentDialogModel;
        }

        public void dismissCurrentDialog(int dismissalCause) {
            mCurrentDialogModel
                    .get(ModalDialogProperties.CONTROLLER)
                    .onDismiss(mCurrentDialogModel, dismissalCause);
        }
    }

    /** Generates a 320x320 test single-colored bitmap. */
    private static Bitmap generateTestBitmap(int color) {
        int width = 320;
        int height = 320;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        Paint paint = new Paint();
        paint.setColor(color);
        paint.setStyle(Paint.Style.FILL);
        canvas.drawPaint(paint);
        return bitmap;
    }

    private View getDialogCustomView() {
        return mDialogManager.getCurrentDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
    }

    private CharSequence getDialogHeaderView() {
        return mDialogManager
                .getCurrentDialogModel()
                .get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
    }

    private String getDialogTitle() {
        return mDialogManager.getCurrentDialogModel().get(ModalDialogProperties.TITLE).toString();
    }

    private Bitmap getUpdateDialogBitmap(int resId) {
        ImageView imageView = getDialogCustomView().findViewById(resId);
        if (imageView.getVisibility() != View.VISIBLE) return null;
        return ((BitmapDrawable) imageView.getDrawable()).getBitmap();
    }

    private String getUpdateDialogAppNameLabel(int resId) {
        TextView textView = getDialogCustomView().findViewById(resId);
        if (textView.getVisibility() != View.VISIBLE) return null;

        return textView.getText().toString();
    }

    private String getUpdateDialogHeaderLabel() {
        return getDialogHeaderView().toString();
    }

    private void onUpdateDialogResult(Integer dialogDismissalCause) {
        mLastDismissalCause = dialogDismissalCause;
        mOnActionCallback.notifyCalled();
    }

    private void onAbuseDialogResult() {
        mOnActionCallback.notifyCalled();
    }

    public void verifyValues(boolean clickAccept, DialogParams dialogParams) {
        int callCount = mOnActionCallback.getCallCount();

        WebApkIconNameUpdateDialog dialog = new WebApkIconNameUpdateDialog();
        // Applying a theme overlay because the context is used to show the dialog, which needs some
        // color attributes to inflate the views.
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        dialog.show(
                context,
                mDialogManager,
                /* packageName= */ "",
                dialogParams.iconChanged,
                dialogParams.shortNameChanged,
                dialogParams.nameChanged,
                dialogParams.shortNameBefore,
                dialogParams.shortNameAfter,
                dialogParams.nameBefore,
                dialogParams.nameAfter,
                dialogParams.bitmapBefore,
                dialogParams.bitmapAfter,
                false,
                false,
                this::onUpdateDialogResult);

        // Verify the short name labels visibility status.
        Assert.assertEquals(
                dialogParams.shortNameChanged || dialogParams.expectShortNameShownAnyway
                        ? dialogParams.shortNameBefore
                        : null,
                getUpdateDialogAppNameLabel(R.id.short_app_name_old));
        Assert.assertEquals(
                dialogParams.shortNameChanged || dialogParams.expectShortNameShownAnyway
                        ? dialogParams.shortNameAfter
                        : null,
                getUpdateDialogAppNameLabel(R.id.short_app_name_new));

        // Verify the app name label visibility status.
        Assert.assertEquals(
                dialogParams.nameChanged && !dialogParams.expectNameHiddenAnyway
                        ? dialogParams.nameBefore
                        : null,
                getUpdateDialogAppNameLabel(R.id.app_name_old));
        Assert.assertEquals(
                dialogParams.nameChanged && !dialogParams.expectNameHiddenAnyway
                        ? dialogParams.nameAfter
                        : null,
                getUpdateDialogAppNameLabel(R.id.app_name_new));

        // Verify the icons visibility status.
        Assert.assertEquals(
                dialogParams.iconChanged || dialogParams.expectIconShownAnyway
                        ? dialogParams.bitmapBefore
                        : null,
                getUpdateDialogBitmap(R.id.app_icon_old));
        Assert.assertEquals(
                dialogParams.iconChanged || dialogParams.expectIconShownAnyway
                        ? dialogParams.bitmapAfter
                        : null,
                getUpdateDialogBitmap(R.id.app_icon_new));

        mDialogManager.dismissCurrentDialog(
                clickAccept
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        Assert.assertEquals(callCount + 1, mOnActionCallback.getCallCount());

        Assert.assertEquals(
                clickAccept
                        ? (Integer) DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : (Integer) DialogDismissalCause.NEGATIVE_BUTTON_CLICKED,
                mLastDismissalCause);

        Assert.assertEquals(
                (dialogParams.hasExplanationString ? MESSAGE_HEADER_LENGTH : 0),
                getUpdateDialogHeaderLabel().length());
    }

    public void verifyReportAbuseValues(
            boolean clickAccept, String shortAppName, String expectedTitle) throws Exception {
        int callCount = mOnActionCallback.getCallCount();

        // Applying a theme overlay because the context is used to show the dialog, which needs some
        // color attributes to inflate the views.
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        WebApkUpdateReportAbuseDialog dialog =
                new WebApkUpdateReportAbuseDialog(
                        context,
                        mDialogManager,
                        /* packageName= */ "",
                        shortAppName,
                        /* showAbuseCheckbox= */ true,
                        this::onAbuseDialogResult);
        dialog.show();

        Assert.assertEquals(expectedTitle, getDialogTitle());

        mDialogManager.dismissCurrentDialog(
                clickAccept
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        // Pressing Cancel on the Abuse dialog does not activate the dismiss-parent callback
        // (because the parent dialog is not supposed to dismiss).
        Assert.assertEquals(callCount + (clickAccept ? 1 : 0), mOnActionCallback.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testCombinations() throws Throwable {
        Bitmap blue = generateTestBitmap(Color.BLUE);
        Bitmap red = generateTestBitmap(Color.RED);

        // Test only icons changing.
        DialogParams dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        // When only the icon changes, the short name is shown (as unchanged) to provide context.
        dialogParams.expectShortNameShownAnyway = true;
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test only short name changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        // When only the short name changes, the icon is shown (as unchanged) to provide context and
        // the explanation string is dropped.
        dialogParams.expectIconShownAnyway = true;
        dialogParams.hasExplanationString = false;
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test only long name changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        // Icons always show, even if unchanged.
        dialogParams.expectIconShownAnyway = true;
        dialogParams.hasExplanationString = false;
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test only short name and icon changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test only name and icon changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test all values changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        verifyValues(/* clickAccept= */ true, dialogParams);

        // Test all values changing, but dialog gets canceled.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        verifyValues(/* clickAccept= */ false, dialogParams);

        // Don't show duplicate name labels (identical change in both name and short name).
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "before";
        dialogParams.shortNameAfter = "after";
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "before";
        dialogParams.nameAfter = "after";
        dialogParams.expectNameHiddenAnyway = true;
        verifyValues(/* clickAccept= */ true, dialogParams);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testReportAbuse() throws Throwable {
        // Make sure the dialog shows the right values.
        verifyReportAbuseValues(/* clickAccept= */ true, "short", "Uninstall 'short'?");

        // Make sure Canceling the dialog does the right thing.
        verifyReportAbuseValues(/* clickAccept= */ false, "short", "Uninstall 'short'?");
    }
}
