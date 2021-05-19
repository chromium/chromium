// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.os.Looper;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/**
 * Test for the dialog warning that WebApks have an updated name/icon.
 */
// TODO(crbug/1209230): Investigate converting this to Roboelectric test.
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkIconNameUpdateDialogTest {
    // A callback that fires when an action is taken in the dialog.
    public final CallbackHelper mOnActionCallback = new CallbackHelper();

    private boolean mLastSeenActionWasAccept;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    static class DialogParams {
        public static DialogParams createDefault() {
            DialogParams dialogParams = new DialogParams();
            dialogParams.iconChanged = false;
            dialogParams.bitmapBefore = null;
            dialogParams.bitmapAfter = null;
            dialogParams.shortNameChanged = false;
            dialogParams.shortNameBefore = "";
            dialogParams.shortNameAfter = "";
            dialogParams.nameChanged = false;
            dialogParams.nameBefore = "";
            dialogParams.nameAfter = "";
            return dialogParams;
        }

        public boolean iconChanged;
        public Bitmap bitmapBefore;
        public Bitmap bitmapAfter;
        public boolean shortNameChanged;
        public String shortNameBefore;
        public String shortNameAfter;
        public boolean nameChanged;
        public String nameBefore;
        public String nameAfter;
    }

    @Before
    public void setUp() {
        Looper.prepare();
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Generates a 320x320 test single-colored bitmap.
     */
    private Bitmap generateTestBitmap(int color) {
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

    private View getUpdateDialogCustomView() {
        return mActivityTestRule.getActivity()
                .getModalDialogManager()
                .getCurrentDialogForTest()
                .get(ModalDialogProperties.CUSTOM_VIEW);
    }

    private Bitmap getUpdateDialogBitmap(int resId) {
        ImageView imageView = getUpdateDialogCustomView().findViewById(resId);
        if (imageView.getVisibility() != View.VISIBLE) return null;
        return ((BitmapDrawable) imageView.getDrawable()).getBitmap();
    }

    private String getUpdateDialogAppNameLabel(int resId) {
        TextView textView = getUpdateDialogCustomView().findViewById(resId);
        if (textView.getVisibility() != View.VISIBLE) return null;

        return textView.getText().toString();
    }

    private void onResult(Integer dialogDismissalCause) {
        mLastSeenActionWasAccept =
                dialogDismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
        mOnActionCallback.notifyCalled();
    }

    public void verifyValues(Boolean expectAccept, DialogParams dialogParams) throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            int callCount = mOnActionCallback.getCallCount();
            ModalDialogManager modalDialogManager =
                    mActivityTestRule.getActivity().getModalDialogManager();

            WebApkIconNameUpdateDialog dialog = new WebApkIconNameUpdateDialog();

            dialog.show(modalDialogManager, dialogParams.iconChanged, dialogParams.shortNameChanged,
                    dialogParams.nameChanged, dialogParams.shortNameBefore,
                    dialogParams.shortNameAfter, dialogParams.nameBefore, dialogParams.nameAfter,
                    dialogParams.bitmapBefore, dialogParams.bitmapAfter, false, false,
                    this::onResult);

            Assert.assertEquals(dialogParams.shortNameChanged ? dialogParams.shortNameBefore : null,
                    getUpdateDialogAppNameLabel(R.id.short_app_name_old));
            Assert.assertEquals(dialogParams.shortNameChanged ? dialogParams.shortNameAfter : null,
                    getUpdateDialogAppNameLabel(R.id.short_app_name_new));
            Assert.assertEquals(dialogParams.nameChanged ? dialogParams.nameBefore : null,
                    getUpdateDialogAppNameLabel(R.id.app_name_old));
            Assert.assertEquals(dialogParams.nameChanged ? dialogParams.nameAfter : null,
                    getUpdateDialogAppNameLabel(R.id.app_name_new));
            Assert.assertEquals(dialogParams.iconChanged ? dialogParams.bitmapBefore : null,
                    getUpdateDialogBitmap(R.id.app_icon_old));
            Assert.assertEquals(dialogParams.iconChanged ? dialogParams.bitmapAfter : null,
                    getUpdateDialogBitmap(R.id.app_icon_new));

            modalDialogManager.getCurrentPresenterForTest().dismissCurrentDialog(expectAccept
                            ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                            : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            Assert.assertEquals(callCount + 1, mOnActionCallback.getCallCount());
        });

        Assert.assertEquals(expectAccept, mLastSeenActionWasAccept);
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
        verifyValues(/* expectAccept= */ true, dialogParams);

        // Test only short name changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        verifyValues(/* expectAccept= */ true, dialogParams);

        // Test only long name changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        verifyValues(/* expectAccept= */ true, dialogParams);

        // Test only short name and icon changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.shortNameChanged = true;
        dialogParams.shortNameBefore = "short1";
        dialogParams.shortNameAfter = "short2";
        verifyValues(/* expectAccept= */ true, dialogParams);

        // Test only name and icon changing.
        dialogParams = DialogParams.createDefault();
        dialogParams.iconChanged = true;
        dialogParams.bitmapBefore = blue;
        dialogParams.bitmapAfter = red;
        dialogParams.nameChanged = true;
        dialogParams.nameBefore = "name1";
        dialogParams.nameAfter = "name2";
        verifyValues(/* expectAccept= */ true, dialogParams);

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
        verifyValues(/* expectAccept= */ true, dialogParams);

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
        verifyValues(/* expectAccept= */ false, dialogParams);
    }
}
