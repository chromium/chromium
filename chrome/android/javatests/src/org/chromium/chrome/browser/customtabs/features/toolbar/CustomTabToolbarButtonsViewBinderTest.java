// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CloseButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.ExecutionException;

/** On-device unit tests for {@link CustomTabToolbarButtonsViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures(ChromeFeatureList.CCT_TOOLBAR_REFACTOR)
public class CustomTabToolbarButtonsViewBinderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private View.OnClickListener mOnClickListener;
    @Mock private MaximizeButtonCallback mMaximizeButtonCallback;

    private Activity mActivity;
    private CustomTabToolbar mToolbar;
    private PropertyModel mModel;
    private PropertyListModel<PropertyModel, PropertyKey> mCustomActionButtons;

    @Before
    public void setUp() throws ExecutionException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar =
                            (CustomTabToolbar)
                                    LayoutInflater.from(mActivity)
                                            .inflate(
                                                    R.layout.new_custom_tab_toolbar,
                                                    new FrameLayout(mActivity),
                                                    false);
                    mActivity.setContentView(mToolbar);

                    mCustomActionButtons = new PropertyListModel<>();
                    mModel =
                            CustomTabToolbarButtonsProperties.create(
                                    /* customActionButtonsVisible= */ true,
                                    mCustomActionButtons,
                                    new MinimizeButtonData(),
                                    new CloseButtonData(),
                                    /* menuButtonVisible= */ true,
                                    /* optionalButtonVisible= */ false,
                                    /* toolbarWidth= */ ViewUtils.dpToPx(mActivity, 500),
                                    /* omniboxEnabled= */ false,
                                    /* titleVisible= */ false,
                                    /* isIncognito= */ false);

                    // The view binder uses this tag to get the model.
                    mToolbar.setTag(R.id.view_model, mModel);

                    CustomTabToolbarButtonsViewBinder viewBinder =
                            new CustomTabToolbarButtonsViewBinder();
                    PropertyModelChangeProcessor.create(mModel, mToolbar, viewBinder);
                    var listMcp =
                            new ListModelChangeProcessor<>(
                                    mCustomActionButtons, mToolbar, viewBinder);
                    mCustomActionButtons.addObserver(listMcp);
                });
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testInitialState() {
        assertNull(mToolbar.getCloseButton());
        assertNull(mToolbar.getMinimizeButton());
        assertNotNull(mToolbar.getMenuButton());
        assertEquals(View.VISIBLE, mToolbar.getMenuButton().getVisibility());
        assertNull(mToolbar.getSideSheetMaximizeButton());
        assertEquals(0, mToolbar.getCustomActionButtonsParent().getChildCount());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testCloseButton() {
        Drawable icon = new BitmapDrawable();
        mModel.set(
                CustomTabToolbarButtonsProperties.CLOSE_BUTTON,
                new CloseButtonData(
                        true,
                        icon,
                        CustomTabsIntent.CLOSE_BUTTON_POSITION_START,
                        mOnClickListener));

        ImageButton closeButton = mToolbar.getCloseButton();
        assertNotNull(closeButton);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
        assertEquals(icon, closeButton.getDrawable());
        closeButton.performClick();
        verify(mOnClickListener).onClick(closeButton);

        // Close button at start, menu at end.
        FrameLayout.LayoutParams closeLp = (FrameLayout.LayoutParams) closeButton.getLayoutParams();
        assertEquals(0, closeLp.getMarginStart());
        assertNotEquals("Gravity should be start.", 0, closeLp.gravity & Gravity.START);
        FrameLayout.LayoutParams menuLp =
                (FrameLayout.LayoutParams) mToolbar.getMenuButton().getLayoutParams();
        assertEquals(0, menuLp.getMarginEnd());
        assertNotEquals("Gravity should be end.", 0, closeLp.gravity & Gravity.END);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testCloseButton_End() {
        mModel.set(
                CustomTabToolbarButtonsProperties.CLOSE_BUTTON,
                new CloseButtonData(
                        true,
                        new ColorDrawable(),
                        CustomTabsIntent.CLOSE_BUTTON_POSITION_END,
                        mOnClickListener));
        ImageButton closeButton = mToolbar.getCloseButton();
        assertNotNull(closeButton);

        // Close button at end, menu at start.
        FrameLayout.LayoutParams closeLp = (FrameLayout.LayoutParams) closeButton.getLayoutParams();
        assertEquals(0, closeLp.getMarginEnd());
        assertNotEquals("Gravity should be end.", 0, closeLp.gravity & Gravity.END);
        FrameLayout.LayoutParams menuLp =
                (FrameLayout.LayoutParams) mToolbar.getMenuButton().getLayoutParams();
        assertEquals(0, menuLp.getMarginStart());
        assertNotEquals("Gravity should be start.", 0, closeLp.gravity & Gravity.START);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testMinimizeButton() {
        mModel.set(
                CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON,
                new MinimizeButtonData(true, mOnClickListener));

        ImageButton minimizeButton = mToolbar.getMinimizeButton();
        assertNotNull(minimizeButton);
        assertEquals(View.VISIBLE, minimizeButton.getVisibility());
        minimizeButton.performClick();
        verify(mOnClickListener).onClick(minimizeButton);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testSideSheetMaximizeButton() {
        mModel.set(
                CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON,
                new SideSheetMaximizeButtonData(true, false, mMaximizeButtonCallback));

        ImageButton maximizeButton = mToolbar.getSideSheetMaximizeButton();
        assertNotNull(maximizeButton);
        assertEquals(View.VISIBLE, maximizeButton.getVisibility());
        maximizeButton.performClick();
        verify(mMaximizeButtonCallback).onClick();
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testCustomActionButtons_addUpdateRemove() {
        // Add
        Drawable icon1 = new ColorDrawable(0xFF0000);
        String description1 = "description1";
        PropertyModel buttonModel =
                new PropertyModel.Builder(CustomTabToolbarButtonsProperties.INDIVIDUAL_BUTTON_KEYS)
                        .with(CustomTabToolbarButtonsProperties.ICON, icon1)
                        .with(CustomTabToolbarButtonsProperties.DESCRIPTION, description1)
                        .with(CustomTabToolbarButtonsProperties.CLICK_LISTENER, mOnClickListener)
                        .build();
        mCustomActionButtons.add(buttonModel);

        FrameLayout parent = mToolbar.getCustomActionButtonsParent();
        assertEquals(1, parent.getChildCount());

        ImageButton button = (ImageButton) parent.getChildAt(0);
        assertThat(button, instanceOf(ImageButton.class));
        assertEquals(description1, button.getContentDescription());
        assertEquals(icon1, button.getDrawable());

        button.performClick();
        verify(mOnClickListener).onClick(button);

        // Update
        Drawable icon2 = new ColorDrawable(0x00FF00);
        String description2 = "description2";

        buttonModel.set(CustomTabToolbarButtonsProperties.ICON, icon2);
        buttonModel.set(CustomTabToolbarButtonsProperties.DESCRIPTION, description2);

        ImageButton button2 = (ImageButton) parent.getChildAt(0);
        assertEquals(icon2, button2.getDrawable());
        assertEquals(description2, button2.getContentDescription());

        // Remove
        mCustomActionButtons.remove(buttonModel);
        assertEquals(0, parent.getChildCount());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testOptionalButton() {

        mToolbar.ensureOptionalButtonInflated();
        mModel.set(CustomTabToolbarButtonsProperties.OPTIONAL_BUTTON_VISIBLE, true);

        View optionalButton = mToolbar.getOptionalButton();
        assertNotNull(optionalButton);
        assertEquals(View.VISIBLE, optionalButton.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testButtonHiding_notEnoughSpace() {
        mModel.set(
                CustomTabToolbarButtonsProperties.CLOSE_BUTTON,
                new CloseButtonData(
                        true,
                        new ColorDrawable(),
                        CustomTabsIntent.CLOSE_BUTTON_POSITION_START,
                        mOnClickListener));
        mModel.set(
                CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON,
                new MinimizeButtonData(true, mOnClickListener));
        mModel.set(
                CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON,
                new SideSheetMaximizeButtonData(true, false, mMaximizeButtonCallback));

        // All buttons should be visible.
        assertNotNull(mToolbar.getCloseButton());
        assertEquals(View.VISIBLE, mToolbar.getCloseButton().getVisibility());
        assertNotNull(mToolbar.getMenuButton());
        assertEquals(View.VISIBLE, mToolbar.getMenuButton().getVisibility());
        assertNotNull(mToolbar.getMinimizeButton());
        assertEquals(View.VISIBLE, mToolbar.getMinimizeButton().getVisibility());
        assertNotNull(mToolbar.getSideSheetMaximizeButton());
        assertEquals(View.VISIBLE, mToolbar.getSideSheetMaximizeButton().getVisibility());

        // Now set a small width.
        mModel.set(
                CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH, ViewUtils.dpToPx(mActivity, 200));

        // With very little space, only the close and menu buttons should be visible.
        assertNotNull(mToolbar.getCloseButton());
        assertEquals(View.VISIBLE, mToolbar.getCloseButton().getVisibility());
        assertNotNull(mToolbar.getMenuButton());
        assertEquals(View.VISIBLE, mToolbar.getMenuButton().getVisibility());
        assertNotNull(mToolbar.getMinimizeButton());
        assertEquals(View.GONE, mToolbar.getMinimizeButton().getVisibility());
        assertNotNull(mToolbar.getSideSheetMaximizeButton());
        assertEquals(View.GONE, mToolbar.getSideSheetMaximizeButton().getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"CustomTabs"})
    public void testIncognito() {
        mModel.set(CustomTabToolbarButtonsProperties.IS_INCOGNITO, true);

        assertNotNull(mToolbar.getIncognitoImageView());
        assertEquals(View.VISIBLE, mToolbar.getIncognitoImageView().getVisibility());
    }
}
