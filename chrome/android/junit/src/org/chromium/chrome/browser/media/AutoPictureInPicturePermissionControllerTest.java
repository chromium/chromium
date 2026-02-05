// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
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
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.test.util.TestViewAndroidDelegate;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link AutoPictureInPicturePermissionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutoPictureInPicturePermissionControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutoPictureInPicturePermissionController.Natives mNativeMock;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private Tab mTab;
    @Mock private ViewGroup mContainerView;

    private static final Runnable NO_OP_CALLBACK = () -> {};

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private WebContents mWebContents;
    private ViewAndroidDelegate mViewAndroidDelegate;

    private AutoPictureInPictureTabHelper mTabHelper;

    @Before
    public void setUp() {
        AutoPictureInPicturePermissionControllerJni.setInstanceForTesting(mNativeMock);
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();

        mWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));

        when(mTab.getWebContents()).thenReturn(mWebContents);
        lenient().when(mWebContents.getLastCommittedUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        // Use TestViewAndroidDelegate to avoid mocking final methods
        mViewAndroidDelegate = new TestViewAndroidDelegate(mContainerView);
        lenient().when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);

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
    public void testShowPrompt_WhenPermissionIsAsk_DisplaysPrompt() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

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
    public void testShowPrompt_SetsAccessibilityImportance() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        // Simulate existing importance.
        when(mContainerView.getImportantForAccessibility())
                .thenReturn(View.IMPORTANT_FOR_ACCESSIBILITY_YES);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

        // Verify that the container view's accessibility importance is set to prevent
        // linear navigation.
        verify(mContainerView)
                .setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);

        // Capture the controller instance
        AutoPictureInPicturePermissionController controller = mTabHelper.getPermissionController();
        Assert.assertNotNull(controller);

        // Dismiss the prompt.
        controller.dismiss();

        // Verify that the original accessibility importance is restored.
        verify(mContainerView).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
    }

    @Test
    public void testShowPrompt_WhenPermissionIsAllow_DoesNotDisplayPrompt() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ALLOW);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_WhenPermissionIsBlock_DoesNotDisplayPrompt() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.BLOCK);

        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_AllowOnce_PreventsSubsequentPrompt() {
        // Show prompt and simulate "Allow Once" click.
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

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
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testShowPrompt_AllowEveryVisit_SetsPermissionToAllow() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

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
    public void testShowPrompt_BlockClick_SetsPermissionToBlockAndClosesPip() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        Runnable closePipCallback = mock(Runnable.class);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, closePipCallback);

        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        AutoPipPermissionDialogView view = (AutoPipPermissionDialogView) rootView.getChildAt(1);

        ButtonCompat blockButton =
                findButton(view, mActivity.getString(R.string.permission_dont_allow));
        Assert.assertNotNull(blockButton);
        blockButton.performClick();

        verify(mNativeMock).setPermissionStatus(eq(mWebContents), eq(ContentSetting.BLOCK));
        verify(closePipCallback).run();
        Assert.assertEquals(0, rootView.getChildCount());
    }

    @Test
    public void testDismiss_CleansUpUIAndHelperState() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

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

    @Test
    public void testIsAutoPictureInPictureInUse_ReflectsNativeState() {
        when(mNativeMock.isAutoPictureInPictureInUse(mWebContents)).thenReturn(true);
        Assert.assertTrue(
                AutoPictureInPicturePermissionController.isAutoPictureInPictureInUse(mWebContents));

        when(mNativeMock.isAutoPictureInPictureInUse(mWebContents)).thenReturn(false);
        Assert.assertFalse(
                AutoPictureInPicturePermissionController.isAutoPictureInPictureInUse(mWebContents));
    }

    @Test
    public void testPrimaryPageChanged_ClearsAllowOnceState() {
        mTabHelper.setHasAllowOnce(true);

        // Capture the observer registered by the helper
        ArgumentCaptor<WebContentsObserver> captor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify((WebContentsObserver.Observable) mWebContents).addObserver(captor.capture());

        // Simulate primary page changed
        captor.getValue().primaryPageChanged(mock(Page.class));

        Assert.assertFalse("Allow once should be cleared on navigation", mTabHelper.hasAllowOnce());
    }

    @Test
    public void testShowPrompt_ReplacesExistingPrompt() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);

        // First prompt
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);
        AutoPictureInPicturePermissionController firstController =
                mTabHelper.getPermissionController();
        Assert.assertNotNull(firstController);
        ViewGroup rootView = mActivity.findViewById(android.R.id.content);
        Assert.assertEquals(2, rootView.getChildCount());

        // Second prompt
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

        // Verify controller was replaced
        AutoPictureInPicturePermissionController secondController =
                mTabHelper.getPermissionController();
        Assert.assertNotNull(secondController);
        Assert.assertNotSame(firstController, secondController);

        // Verify UI is still correct (old views removed, new ones added).
        // If old views weren't removed, we'd have 4 views.
        Assert.assertEquals(2, rootView.getChildCount());
    }

    @Test
    public void testHandleWindowDestruction_CallsNativeMethodAndDismisses() {
        when(mNativeMock.getPermissionStatus(mWebContents)).thenReturn(ContentSetting.ASK);
        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                mActivity, mTab, NO_OP_CALLBACK);

        Assert.assertNotNull(mTabHelper.getPermissionController());

        AutoPictureInPicturePermissionController.handleWindowDestruction(mWebContents);

        verify(mNativeMock).onPictureInPictureDismissed(mWebContents);
        Assert.assertNull(mTabHelper.getPermissionController());
    }

    @Test
    public void testHandleWindowDestruction_NoActivePrompt_DoesNothing() {
        AutoPictureInPicturePermissionController.handleWindowDestruction(mWebContents);

        verify(mNativeMock, org.mockito.Mockito.never()).onPictureInPictureDismissed(mWebContents);
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
