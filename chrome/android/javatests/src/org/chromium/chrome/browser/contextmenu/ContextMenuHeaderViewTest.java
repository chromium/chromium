// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import static org.chromium.ui.base.DeviceFormFactor.PHONE;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for ContextMenuHeader view and {@link ContextMenuHeaderViewBinder} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuHeaderViewTest {
    private static final String SHORT_TITLE_STRING = "Some Very Cool Title";
    private static final String LONG_TITLE_STRING =
            "Some Very Cool Title Which Will Definitely Need To Be Ellipsized"
                + " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789";
    private static final String SHORT_URL_STRING = "www.website.com";
    private static final String LONG_URL_STRING =
            "www.website.com/ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789";
    private static final String SECONDARY_URL_STRING = "cct.website.com";
    private static final String TERTIARY_URL_STRING = "cct.website.com/test";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private View mHeaderView;
    private TextView mTitle;
    private TextView mUrl;
    private TextView mSecondaryUrl;
    private TextView mTertiaryUrl;
    private ContextMenuHeaderTextView mHeaderTextView;
    private ImageView mImage;
    private View mCircleBg;
    private View mImageContainer;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.context_menu_header);
                    mHeaderView = sActivity.findViewById(android.R.id.content);
                    mTitle = mHeaderView.findViewById(R.id.menu_header_title);
                    mUrl = mHeaderView.findViewById(R.id.menu_header_url);
                    mSecondaryUrl = mHeaderView.findViewById(R.id.menu_header_secondary_url);
                    mTertiaryUrl = mHeaderView.findViewById(R.id.menu_header_tertiary_url);
                    mHeaderTextView = mHeaderView.findViewById(R.id.title_and_url);
                    mImage = mHeaderView.findViewById(R.id.menu_header_image);
                    mCircleBg = mHeaderView.findViewById(R.id.circle_background);
                    mImageContainer = mHeaderView.findViewById(R.id.menu_header_image_container);
                    mModel =
                            new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                                    .with(ListMenuItemProperties.TITLE, "")
                                    .with(ContextMenuHeaderProperties.URL, "")
                                    .with(ContextMenuHeaderProperties.SECONDARY_URL, "")
                                    .with(ContextMenuHeaderProperties.TERTIARY_URL, "")
                                    .with(
                                            ContextMenuHeaderProperties
                                                    .TITLE_AND_URL_CLICK_LISTENER,
                                            null)
                                    .with(ContextMenuHeaderProperties.IMAGE, null)
                                    .with(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                                    .build();

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel, mHeaderView, ContextMenuHeaderViewBinder::bind);
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
    }

    @Test
    @SmallTest
    public void testTitle() {
        assertThat(
                "Incorrect initial title visibility.", mTitle.getVisibility(), equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ListMenuItemProperties.TITLE, SHORT_TITLE_STRING);
                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 2);
                });

        assertThat("Incorrect title visibility.", mTitle.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect title string.", mTitle.getText(), equalTo(SHORT_TITLE_STRING));
        assertThat("Incorrect max line count for title.", mTitle.getMaxLines(), equalTo(2));
        assertThat(
                "Incorrect title ellipsize mode.",
                mTitle.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }

    @Test
    @SmallTest
    public void testUrl() {
        assertThat("Incorrect initial URL visibility.", mUrl.getVisibility(), equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.URL, SHORT_URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                });

        assertThat("Incorrect URL visibility.", mUrl.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect URL string.", mUrl.getText(), equalTo(SHORT_URL_STRING));
        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect URL ellipsize mode.",
                mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE));

        assertThat(
                "Incorrect max line count for URL.",
                mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());
    }

    @Test
    @SmallTest
    public void testSecondaryUrl() {
        assertThat(
                "Incorrect initial secondary URL visibility.",
                mSecondaryUrl.getVisibility(),
                equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.SECONDARY_URL, SECONDARY_URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, 1);
                });

        assertThat(
                "Incorrect secondary URL visibility.",
                mSecondaryUrl.getVisibility(),
                equalTo(View.VISIBLE));
        assertThat(
                "Incorrect secondary URL string.",
                mSecondaryUrl.getText(),
                equalTo(SECONDARY_URL_STRING));
        assertThat(
                "Incorrect max line count for secondary URL.",
                mSecondaryUrl.getMaxLines(),
                equalTo(1));
        assertThat(
                "Incorrect secondary URL ellipsize mode.",
                mSecondaryUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mModel.set(
                                ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                                Integer.MAX_VALUE));

        assertThat(
                "Incorrect max line count for secondary URL.",
                mSecondaryUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull(
                "Secondary URL is ellipsized when it shouldn't be.", mSecondaryUrl.getEllipsize());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.SECONDARY_URL, "");
                });
        assertThat(
                "Secondary URL should be GONE when text is empty.",
                mSecondaryUrl.getVisibility(),
                equalTo(View.GONE));
    }

    @Test
    @SmallTest
    public void testTertiaryUrl() {
        assertThat(
                "Incorrect initial tertiary URL visibility.",
                mTertiaryUrl.getVisibility(),
                equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.TERTIARY_URL, TERTIARY_URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, 1);
                });

        assertThat(
                "Incorrect tertiary URL visibility.",
                mTertiaryUrl.getVisibility(),
                equalTo(View.VISIBLE));
        assertThat(
                "Incorrect tertiary URL string.",
                mTertiaryUrl.getText(),
                equalTo(TERTIARY_URL_STRING));
        assertThat(
                "Incorrect max line count for tertiary URL.",
                mTertiaryUrl.getMaxLines(),
                equalTo(1));
        assertThat(
                "Incorrect tertiary URL ellipsize mode.",
                mTertiaryUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mModel.set(
                                ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES,
                                Integer.MAX_VALUE));

        assertThat(
                "Incorrect max line count for tertiary URL.",
                mTertiaryUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("Tertiary URL is ellipsized when it shouldn't be.", mTertiaryUrl.getEllipsize());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.TERTIARY_URL, "");
                });
        assertThat(
                "Tertiary URL should be GONE when text is empty.",
                mTertiaryUrl.getVisibility(),
                equalTo(View.GONE));
    }

    @Test
    @SmallTest
    @Restriction(PHONE)
    public void testTitleAndUrlClick() {
        // Clicking on the title or the URL expands/shrinks both of them.
        assertFalse(
                "HeaderTextView has onClickListeners when it shouldn't yet.",
                mHeaderTextView.hasOnClickListeners());

        setupForCollapsedState(/* longTitle= */ true, /* longUrl= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHeaderTextView.callOnClick();
                });

        verifyExpandedState();

        ThreadUtils.runOnUiThreadBlocking(mHeaderTextView::callOnClick);

        verifyCollapsedState();
    }

    @Test
    @SmallTest
    public void testImage() {
        assertThat(
                "Incorrect initial circle background visibility.",
                mCircleBg.getVisibility(),
                equalTo(View.INVISIBLE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, true));
        assertThat(
                "Incorrect circle background visibility.",
                mCircleBg.getVisibility(),
                equalTo(View.VISIBLE));

        assertFalse(
                "Thumbnail drawable should use fallback color initially.",
                mImage.getDrawable() instanceof BitmapDrawable);
        final Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.IMAGE, bitmap));
        assertThat(
                "Incorrect thumbnail bitmap.",
                ((BitmapDrawable) mImage.getDrawable()).getBitmap(),
                equalTo(bitmap));
    }

    @Test
    @SmallTest
    @Restriction(PHONE)
    public void testEllipsizedText_longTitle_isExpandable() {
        setupForCollapsedState(/* longTitle= */ true, /* longUrl= */ false);

        assertThat(
                "Click listener should be set when text is ellipsized.",
                mHeaderTextView.hasOnClickListeners());
    }

    @Test
    @SmallTest
    @Restriction(PHONE)
    public void testNonEllipsizedText_isNotExpandable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ListMenuItemProperties.TITLE, SHORT_TITLE_STRING);
                    mModel.set(
                            ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER, view -> {});
                });

        assertThat(
                "Click listener should not be set when text is not ellipsized.",
                !mHeaderTextView.hasOnClickListeners());
    }

    @Test
    @SmallTest
    @Restriction(PHONE)
    public void testAccessibility() {
        setupForCollapsedState(/* longTitle= */ true, /* longUrl= */ false);
        ThreadUtils.runOnUiThreadBlocking(mHeaderTextView::callOnClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollUiThread(
                () -> mTitle.getLayout().getEllipsisCount(mTitle.getLineCount() - 1) == 0);
        verifyExpandedState();
        assertThat(
                "Context menu header should have IS_EXPANDED set to true",
                mModel.get(ContextMenuHeaderProperties.IS_EXPANDED));
        AccessibilityNodeInfo info = mHeaderTextView.createAccessibilityNodeInfo();
        mHeaderTextView.onInitializeAccessibilityNodeInfo(info);
        assertThat(
                "ACTION_COLLAPSE should be available when expanded.",
                info.getActionList(),
                hasItem(ACTION_COLLAPSE));
        ThreadUtils.runOnUiThreadBlocking(mHeaderTextView::callOnClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollUiThread(
                () -> mTitle.getLayout().getEllipsisCount(mTitle.getLineCount() - 1) > 0);
        verifyCollapsedState();
        assertThat(
                "Context menu header should have IS_EXPANDED set to false",
                !mModel.get(ContextMenuHeaderProperties.IS_EXPANDED));
        assertThat(
                "ACTION_EXPAND should be available when collapsed.",
                mHeaderTextView.createAccessibilityNodeInfo().getActionList(),
                hasItem(ACTION_EXPAND));
    }

    @Test
    @SmallTest
    @Restriction(PHONE)
    public void testPerformAccessibilityAction() {
        setupForCollapsedState(/* longTitle= */ true, /* longUrl= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mHeaderTextView.performAccessibilityAction(ACTION_EXPAND.getId(), null));
        verifyExpandedState();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHeaderTextView.performAccessibilityAction(ACTION_COLLAPSE.getId(), null));
        verifyCollapsedState();
    }

    private void setupForCollapsedState(boolean longTitle, boolean longUrl) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ListMenuItemProperties.TITLE,
                            longTitle ? LONG_TITLE_STRING : SHORT_TITLE_STRING);
                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1);
                    mModel.set(
                            ContextMenuHeaderProperties.URL,
                            longUrl ? LONG_URL_STRING : SHORT_URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                    mModel.set(
                            ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER,
                            (v) -> {
                                if (mModel.get(ContextMenuHeaderProperties.URL_MAX_LINES)
                                        == Integer.MAX_VALUE) {
                                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1);
                                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                                    mModel.set(ContextMenuHeaderProperties.IS_EXPANDED, false);
                                } else {
                                    mModel.set(
                                            ContextMenuHeaderProperties.TITLE_MAX_LINES,
                                            Integer.MAX_VALUE);
                                    mModel.set(
                                            ContextMenuHeaderProperties.URL_MAX_LINES,
                                            Integer.MAX_VALUE);
                                    mModel.set(ContextMenuHeaderProperties.IS_EXPANDED, true);
                                }
                            });
                });
        CriteriaHelper.pollUiThread(mHeaderTextView::hasOnClickListeners);
    }

    private void verifyExpandedState() {
        assertThat(
                "Incorrect max line count for title.",
                mTitle.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("Title is ellipsized when it shouldn't be.", mTitle.getEllipsize());
        assertThat(
                "Incorrect max line count for URL.",
                mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());
    }

    private void verifyCollapsedState() {
        assertThat("Incorrect max line count for title.", mTitle.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect title ellipsize mode.",
                mTitle.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect URL ellipsize mode.",
                mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }
}
