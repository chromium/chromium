// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.os.Looper;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link AutoPictureInPicturePermissionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutoPictureInPicturePermissionControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutoPictureInPicturePermissionController.Natives mNativeMock;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private Tab mTab;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    @Mock private WebContents mWebContents;

    private AutoPictureInPictureTabHelper mTabHelper;

    @Before
    public void setUp() {
        AutoPictureInPicturePermissionControllerJni.setInstanceForTesting(mNativeMock);
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();

        when(mTab.getWebContents()).thenReturn(mWebContents);
        lenient().when(mWebContents.getLastCommittedUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        mTabHelper = new AutoPictureInPictureTabHelper(mWebContents);
        lenient()
                .doReturn(mTabHelper)
                .when(mWebContents)
                .getOrSetUserData(eq(AutoPictureInPictureTabHelper.USER_DATA_KEY), any());

        lenient()
                .when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenReturn("example.com");
    }

    @After
    public void tearDown() {
        if (mActivityController != null) {
            mActivityController.pause().stop().destroy();
        }
    }

    @Test
    public void testShowPrompt_Ask() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        // Verify the prompt is added to the root view.
        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        // Expect 2 views added: Mask and Dialog.
        Assert.assertEquals(2, rootView.getChildCount());
        Assert.assertTrue(rootView.getChildAt(0) instanceof AutoPictureInPicturePrivacyMaskView);
        Assert.assertTrue(rootView.getChildAt(1) instanceof AutoPipPermissionDialogView);

        // Verify controller registration
        Assert.assertNotNull(mTabHelper.getPermissionController());
    }

    @Test
    public void testShowPrompt_Allow() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ALLOW);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_Block() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.BLOCK);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_AlreadyAllowedOnce() {
        // Show prompt and simulate "Allow Once" click.
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        AutoPipPermissionDialogView view = (AutoPipPermissionDialogView) rootView.getChildAt(1);

        ButtonCompat allowOnceButton =
                findButton(view, mActivity.getString(R.string.permission_allow_this_time));
        Assert.assertNotNull(allowOnceButton);
        allowOnceButton.performClick();
        shadowOf(Looper.getMainLooper()).idle();

        // Verify view is removed.
        Assert.assertEquals(0, rootView.getChildCount());

        // Should not show prompt.
        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_AllowEveryVisit() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        AutoPipPermissionDialogView view = (AutoPipPermissionDialogView) rootView.getChildAt(1);

        ButtonCompat allowButton =
                findButton(view, mActivity.getString(R.string.permission_allow_every_visit));
        Assert.assertNotNull(allowButton);
        allowButton.performClick();

        verify(mNativeMock).setPermissionStatus(eq(mWebContents), eq(ContentSetting.ALLOW));
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_BlockClick() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        AutoPipPermissionDialogView view = (AutoPipPermissionDialogView) rootView.getChildAt(1);

        ButtonCompat blockButton =
                findButton(view, mActivity.getString(R.string.permission_dont_allow));
        Assert.assertNotNull(blockButton);
        blockButton.performClick();

        verify(mNativeMock).setPermissionStatus(eq(mWebContents), eq(ContentSetting.BLOCK));
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testDismiss_CleansUpState() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(mActivity, mTab);

        // Capture the controller instance
        AutoPictureInPicturePermissionController controller = mTabHelper.getPermissionController();
        Assert.assertNotNull(controller);

        // Force a dismiss (simulating navigation or tab close)
        controller.dismiss();

        // Verify UI is gone
        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(0, rootView.getChildCount());

        // Verify Helper reference is cleared
        Assert.assertNull(
                "Helper should release reference after dismiss",
                mTabHelper.getPermissionController());
    }

    private ButtonCompat findButton(AutoPipPermissionDialogView view, String text) {
        ViewGroup buttonContainer =
                view.findViewById(org.chromium.chrome.R.id.auto_pip_button_container);
        for (int i = 0; i < buttonContainer.getChildCount(); i++) {
            ButtonCompat button = (ButtonCompat) buttonContainer.getChildAt(i);
            if (text.equals(button.getText())) {
                return button;
            }
        }
        return null;
    }
}
