// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.app.Activity;
import android.content.Context;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View.MeasureSpec;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

@RunWith(BaseRobolectricTestRunner.class)
// Native graphics are needed for real text measurement / ellipsizing.
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class EducationalTipModuleViewUnitTest {
    private static final String LONG_TITLE =
            "This is a very long title that cannot possibly fit on a single line of the module";
    private static final int MODULE_WIDTH_PX = 300;

    private EducationalTipModuleView mModuleView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        mModuleView =
                (EducationalTipModuleView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.educational_tip_module_layout, null);
    }

    @After
    public void tearDown() {
        mModuleView.destroyForTesting();
        mModuleView = null;
    }

    @Test
    public void testSetContentTitleAndDescription() {
        String testTitle1 = "This is a test title";
        String testTitle2 = "Here is another test title";

        TextView contentTitleView =
                mModuleView.findViewById(R.id.educational_tip_module_content_title);
        TextView contentDescriptionView =
                mModuleView.findViewById(R.id.educational_tip_module_content_description);

        Assert.assertEquals("", contentTitleView.getText());
        mModuleView.setContentTitle(testTitle1);
        Assert.assertEquals(testTitle1, contentTitleView.getText());
        mModuleView.setContentTitle(testTitle2);
        Assert.assertEquals(testTitle2, contentTitleView.getText());

        Assert.assertEquals("", contentDescriptionView.getText());
        mModuleView.setContentDescription(testTitle1);
        Assert.assertEquals(testTitle1, contentDescriptionView.getText());
        mModuleView.setContentDescription(testTitle2);
        Assert.assertEquals(testTitle2, contentDescriptionView.getText());
    }

    @Test
    public void testUpdateContentTitleAndDescriptionMaxLines() {
        Assert.assertTrue(mModuleView.getIsTitleSingleLineForTesting());
        TextView contentTitleView =
                mModuleView.findViewById(R.id.educational_tip_module_content_title);
        TextView contentDescriptionView =
                mModuleView.findViewById(R.id.educational_tip_module_content_description);

        // Test if the title exceeds the available horizontal space, wrap it to two lines and limit
        // the description to a single line.
        mModuleView.setContentTitle(LONG_TITLE);
        layoutModuleView();
        mModuleView.updateContentTitleAndDescriptionMaxLines();
        Assert.assertEquals(2, contentTitleView.getMaxLines());
        Assert.assertEquals(1, contentDescriptionView.getMaxLines());
        Assert.assertFalse(mModuleView.getIsTitleSingleLineForTesting());

        // Test if the title fits within a single line, the description should span two lines.
        mModuleView.setContentTitle("Title");
        layoutModuleView();
        mModuleView.updateContentTitleAndDescriptionMaxLines();
        Assert.assertEquals(1, contentTitleView.getMaxLines());
        Assert.assertEquals(2, contentDescriptionView.getMaxLines());
        Assert.assertTrue(mModuleView.getIsTitleSingleLineForTesting());
    }

    @Test
    public void testOnLayoutChangeListener() {
        TextView contentTitleView =
                mModuleView.findViewById(R.id.educational_tip_module_content_title);
        mModuleView.setContentTitle(LONG_TITLE);

        // The view must be attached to a window for View#post() runnables to run.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setContentView(mModuleView);
        Assert.assertEquals(1, contentTitleView.getMaxLines());

        // Laying out the title triggers the layout change listener, which posts an update of the
        // max lines.
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(2, contentTitleView.getMaxLines());
        Assert.assertFalse(mModuleView.getIsTitleSingleLineForTesting());
    }

    @Test
    public void testSetCompleted_True() {
        mModuleView.setCompleted(true);
        verifySetCompleted();
    }

    @Test
    public void testSetCompleted_False() {
        // Call setCompleted(true) first to change from default
        mModuleView.setCompleted(true);
        // Then call setCompleted(false) to attempt to reset
        mModuleView.setCompleted(false);

        // In the current implementation, setCompleted(false) doesn't revert the changes.
        // So, the styles should still be the disabled ones.
        verifySetCompleted();
    }

    @Test
    public void testSetUseTransparentIconBackground() {
        ImageView imageView = mModuleView.findViewById(R.id.educational_tip_module_content_image);
        Drawable background = imageView.getBackground();
        Assert.assertNotNull(background);

        mModuleView.setUseTransparentIconBackground(true);
        Assert.assertNull(imageView.getBackground());
    }

    private void layoutModuleView() {
        mModuleView.measure(
                MeasureSpec.makeMeasureSpec(MODULE_WIDTH_PX, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        mModuleView.layout(0, 0, MODULE_WIDTH_PX, mModuleView.getMeasuredHeight());
    }

    private void verifySetCompleted() {
        TextView contentTitleView =
                mModuleView.findViewById(R.id.educational_tip_module_content_title);
        TextView contentDescriptionView =
                mModuleView.findViewById(R.id.educational_tip_module_content_description);
        TextView buttonView = mModuleView.findViewById(R.id.educational_tip_module_button);

        int disabledColor = mContext.getColor(R.color.default_text_color_disabled_list);

        Assert.assertEquals(disabledColor, contentTitleView.getCurrentTextColor());
        Assert.assertTrue((contentTitleView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG) != 0);
        Assert.assertEquals(disabledColor, contentDescriptionView.getCurrentTextColor());
        Assert.assertTrue(
                (contentDescriptionView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG) != 0);

        Assert.assertFalse(buttonView.isEnabled());
        Assert.assertEquals(disabledColor, buttonView.getCurrentTextColor());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Assert.assertEquals(
                    mContext.getString(R.string.educational_tip_accessibility_item_completed),
                    mModuleView.getStateDescription());
        }
    }
}
