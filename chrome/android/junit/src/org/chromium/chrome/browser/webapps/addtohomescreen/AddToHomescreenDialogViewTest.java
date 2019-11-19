// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.app.Activity;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for the {@link AddToHomescreenDialogView} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AddToHomescreenDialogViewTest {
    private AddToHomescreenDialogView mAddToHomescreenDialogView;
    private CallbackHelper mAddCallback = new CallbackHelper();
    private CallbackHelper mTitleClickCallback = new CallbackHelper();
    private CallbackHelper mDismissCallback = new CallbackHelper();
    private MockModalDialogManager mModalDialogManager = new MockModalDialogManager();

    private static final String TEST_TITLE = "YouTube";
    private static final String TEST_URL = "youtube.com";
    private static final String TEST_NATIVE_ADD_TEXT = "Install";

    private class MockModalDialogManager extends ModalDialogManager {
        private PropertyModel mShownDialogModel;
        private PropertyModel mDismissedDialogModel;
        private int mDismissalCause;

        public MockModalDialogManager() {
            super(Mockito.mock(Presenter.class), 0);
        }

        @Override
        public void showDialog(PropertyModel model, int dialogType) {
            mShownDialogModel = model;
        }

        @Override
        public void dismissDialog(PropertyModel model, int dismissalCause) {
            mDismissedDialogModel = model;
            mDismissalCause = dismissalCause;
            model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
        }

        public @DialogDismissalCause int getDismissalCause() {
            return mDismissalCause;
        }

        public PropertyModel getDismissedDialogModel() {
            return mDismissedDialogModel;
        }

        public PropertyModel getShownDialogModel() {
            return mShownDialogModel;
        }
    }

    @Before
    public void setUp() {
        // Create and show the view.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mAddToHomescreenDialogView = new AddToHomescreenDialogView(activity, mModalDialogManager,
                R.string.menu_add_to_homescreen, new AddToHomescreenViewDelegate() {
                    @Override
                    public void onAddToHomescreen(String title) {
                        mAddCallback.notifyCalled();
                    }

                    @Override
                    public boolean onAppDetailsRequested() {
                        mTitleClickCallback.notifyCalled();
                        return true;
                    }

                    @Override
                    public void onViewDismissed() {
                        mDismissCallback.notifyCalled();
                    }
                });
    }

    @Test
    @Feature({"Webapp"})
    public void testLoadingState() {
        // Assert dialog is showing.
        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(shownDialogModel);

        // Assert views exist.
        View parentView = mAddToHomescreenDialogView.getParentViewForTest();
        Assert.assertNotNull(parentView);
        Assert.assertNotNull(parentView.findViewById(R.id.spinny));
        Assert.assertNotNull(parentView.findViewById(R.id.icon));
        Assert.assertNotNull(parentView.findViewById(R.id.text));
        Assert.assertNotNull(parentView.findViewById(R.id.app_info));
        Assert.assertNotNull(parentView.findViewById(R.id.name));
        Assert.assertNotNull(parentView.findViewById(R.id.origin));
        Assert.assertNotNull(parentView.findViewById(R.id.control_rating));
        Assert.assertNotNull(parentView.findViewById(R.id.play_logo));

        // Visibility test.
        assertVisibility(R.id.spinny, true);
        assertVisibility(R.id.icon, false);
        assertVisibility(R.id.text, false);
        assertVisibility(R.id.app_info, false);

        // Assert dialog buttons text.
        Assert.assertEquals(
                "Add", shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Cancel", shownDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        // Assert 'Add' is disabled and 'Cancel' is enabled.
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        Assert.assertFalse(shownDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
    }

    /**
     * Tests the view for {@link AppType#WEBAPK}.
     */
    @Test
    @Feature({"Webapp"})
    public void testWebAPK() {
        initDialogView(AppType.WEBAPK);
        mAddToHomescreenDialogView.setUrl(TEST_URL);

        assertVisibility(R.id.spinny, false);
        assertVisibility(R.id.icon, true);
        assertVisibility(R.id.app_info, true);
        assertVisibility(R.id.text, false);
        assertVisibility(R.id.origin, true);
        assertVisibility(R.id.control_rating, false);
        assertVisibility(R.id.play_logo, false);

        Assert.assertEquals(TEST_TITLE, getTextForViewWithId(R.id.name));
        Assert.assertEquals(TEST_URL, getTextForViewWithId(R.id.origin));
    }

    /**
     * Tests the view for {@link AppType#SHORTCUT}.
     */
    @Test
    @Feature({"Webapp"})
    public void testShortcut() {
        initDialogView(AppType.SHORTCUT);

        assertVisibility(R.id.spinny, false);
        assertVisibility(R.id.icon, true);
        assertVisibility(R.id.app_info, false);
        assertVisibility(R.id.text, true);
        assertVisibility(R.id.origin, false);
        assertVisibility(R.id.control_rating, false);
        assertVisibility(R.id.play_logo, false);

        Assert.assertEquals(TEST_TITLE, getTextForViewWithId(R.id.text));
    }

    /**
     * Tests the view for {@link AppType#NATIVE}.
     */
    @Test
    @Feature({"Webapp"})
    public void testNativeApp() {
        initDialogView(AppType.NATIVE);
        mAddToHomescreenDialogView.setNativeAppRating(2.3f);
        mAddToHomescreenDialogView.setNativeInstallButtonText(TEST_NATIVE_ADD_TEXT);

        assertVisibility(R.id.spinny, false);
        assertVisibility(R.id.icon, true);
        assertVisibility(R.id.app_info, true);
        assertVisibility(R.id.text, false);
        assertVisibility(R.id.origin, false);
        assertVisibility(R.id.control_rating, true);
        assertVisibility(R.id.play_logo, true);

        Assert.assertEquals(TEST_TITLE, getTextForViewWithId(R.id.text));

        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertEquals(TEST_NATIVE_ADD_TEXT,
                shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
    }

    @Test
    @Feature({"Webapp"})
    public void testAddButtonState() {
        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();

        // Assert 'Add' will be enabled for AppType#WEBAPK after #setCanSubmit(true) is called.
        mAddToHomescreenDialogView.setType(AppType.WEBAPK);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(true);
        Assert.assertFalse(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(false);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        // Assert 'Add' will be enabled for AppType#NATIVE after #setCanSubmit(true) is called.
        mAddToHomescreenDialogView.setType(AppType.NATIVE);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(true);
        Assert.assertFalse(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(false);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        // Assert 'Add' will be enabled for AppType#SHORTCUT after #setCanSubmit(true) is called and
        // title EditText is not empty.
        EditText titleText =
                mAddToHomescreenDialogView.getParentViewForTest().findViewById(R.id.text);

        mAddToHomescreenDialogView.setType(AppType.SHORTCUT);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(true);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(false);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        titleText.setText(TEST_TITLE);
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        mAddToHomescreenDialogView.setCanSubmit(true);
        Assert.assertFalse(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        titleText.setText("");
        Assert.assertTrue(shownDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    /**
     * Tests whether the callback for clicking on the title or icon functions correctly.
     */
    @Test
    @Feature({"Webapp"})
    public void testTitleClickCallback() {
        initDialogView(AppType.NATIVE);

        Assert.assertEquals(0, mTitleClickCallback.getCallCount());
        mAddToHomescreenDialogView.getParentViewForTest().findViewById(R.id.name).performClick();
        mAddToHomescreenDialogView.getParentViewForTest().findViewById(R.id.icon).performClick();
        Assert.assertEquals(2, mTitleClickCallback.getCallCount());
        Assert.assertEquals(2, mDismissCallback.getCallCount());
        Assert.assertNotNull(mModalDialogManager.getDismissedDialogModel());
        Assert.assertEquals(
                mModalDialogManager.getDismissalCause(), DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * Tests whether the callback for dismissal functions correctly.
     */
    @Test
    @Feature({"Webapp"})
    public void testDismissCallback() {
        initDialogView(AppType.NATIVE);

        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertEquals(0, mDismissCallback.getCallCount());
        shownDialogModel.get(ModalDialogProperties.CONTROLLER)
                .onClick(shownDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Assert.assertEquals(1, mDismissCallback.getCallCount());
    }

    /**
     * Tests whether the callback for clicking on the 'Add' button functions correctly.
     */
    @Test
    @Feature({"Webapp"})
    public void testInstallCallback() {
        initDialogView(AppType.WEBAPK);
        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertEquals(0, mAddCallback.getCallCount());
        shownDialogModel.get(ModalDialogProperties.CONTROLLER)
                .onClick(shownDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Assert.assertEquals(1, mAddCallback.getCallCount());
    }

    private void initDialogView(@AppType int appType) {
        mAddToHomescreenDialogView.setType(appType);
        mAddToHomescreenDialogView.setTitle(TEST_TITLE);
        mAddToHomescreenDialogView.setIcon(null, false);
        mAddToHomescreenDialogView.setCanSubmit(true);
    }

    private void assertVisibility(int viewId, boolean isVisible) {
        Assert.assertEquals(isVisible,
                View.VISIBLE
                        == mAddToHomescreenDialogView.getParentViewForTest()
                                   .findViewById(viewId)
                                   .getVisibility());
    }

    private String getTextForViewWithId(int viewId) {
        return ((TextView) mAddToHomescreenDialogView.getParentViewForTest().findViewById(viewId))
                .getText()
                .toString();
    }
}
