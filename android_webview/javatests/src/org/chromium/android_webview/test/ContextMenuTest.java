// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;

import static org.hamcrest.Matchers.equalTo;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;

import androidx.activity.ComponentDialog;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.contextmenu.AwContextMenuCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHeaderCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHelper;
import org.chromium.android_webview.contextmenu.AwContextMenuPopulator;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuSwitches;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for context menu methods */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU})
public class ContextMenuTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mRule;

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private Context mContext;
    private AwContextMenuHelper mHelper;
    private AwContextMenuCoordinator mCoordinator;

    public ContextMenuTest(AwSettingsMutation param) {
        mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        TestAwContentsClient mContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        mContext = mAwContents.getWebContents().getTopLevelNativeWindow().getContext().get();
        mHelper = new AwContextMenuHelper(mAwContents.getWebContents());
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() {
        mHelper = null;
        mCoordinator = null;
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCopyLinkText() throws Throwable {
        ContextMenuParams params =
                createContextMenuParams(ContextMenuDataMediaType.NONE, true, "Test Link", "", "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links", mCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.clickListItemForTesting(R.id.contextmenu_copy_link_text));

        Assert.assertEquals("Test Link", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCopyLinkURL() throws Throwable {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        true,
                        "Test Link",
                        "http://www.test_link.html/",
                        "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links", mCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.clickListItemForTesting(R.id.contextmenu_copy_link_address));

        Assert.assertEquals("http://www.test_link.html/", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCopyLinkURLWithImage() throws Throwable {
        // In a link with a nested image, copy link should copy the URL of the anchor link and not
        // the src of the image.
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.IMAGE,
                        true,
                        "Test Link Image",
                        "http://www.test_link.html/",
                        "http://www.image_source.html/");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links with images", mCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.clickListItemForTesting(R.id.contextmenu_copy_link_address));

        Assert.assertEquals("http://www.test_link.html/", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOpenInBrowser() throws Throwable {
        try {
            Intents.init();
            // Before triggering the viewing intent, stub it out to avoid cascading that into
            // further intents and opening the web browser.
            intending(IntentMatchers.hasAction(equalTo(Intent.ACTION_VIEW)))
                    .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

            ContextMenuParams params =
                    createContextMenuParams(
                            ContextMenuDataMediaType.NONE,
                            true,
                            "",
                            "http://www.test_link.html/",
                            "");
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mHelper.showContextMenu(params, mTestContainerView));

            mCoordinator = mHelper.getCoordinatorForTesting();
            Assert.assertNotNull("Context menu should be created for links", mCoordinator);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> mCoordinator.clickListItemForTesting(R.id.contextmenu_open_link_id));

            intended(IntentMatchers.hasAction(equalTo(Intent.ACTION_VIEW)));
        } finally {
            Intents.release();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testDismissContextMenuOnBack_dialog() throws Throwable {
        doTestDismissContextMenuOnBack(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP) /* for non-tablet devices */
    public void testDismissContextMenuOnBack_popup() throws Throwable {
        doTestDismissContextMenuOnBack(true);
    }

    public void doTestDismissContextMenuOnBack(boolean isPopup) throws Exception {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE, true, "", "http://www.test_link.html/", "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links", mCoordinator);

        AnchoredPopupWindow popupWindow = mCoordinator.getPopupWindowForTesting();
        ComponentDialog dialog = mCoordinator.getDialogForTesting();

        if (isPopup) {
            Assert.assertNotNull("Popup menu should be created for links", popupWindow);
            Assert.assertTrue(popupWindow.isShowing());
        } else {
            Assert.assertNotNull("Dialog menu should be created for links", dialog);
            Assert.assertTrue(dialog.isShowing());
            CriteriaHelper.pollUiThread(
                    () -> {
                        return !mRule.getActivity().hasWindowFocus();
                    },
                    "Context menu should have window focus");
        }

        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressBack();

        // For the dialog menu, we can rely on window regaining focus since dialogs take over the
        // whole screen but for not for the dropdown menu since it is a popup.
        CriteriaHelper.pollUiThread(
                () -> {
                    return isPopup
                            ? mCoordinator.getPopupWindowForTesting() == null
                            : mRule.getActivity().hasWindowFocus();
                },
                "Activity should have regained focus.");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testDismissContextMenuOnClick_dialog() throws Throwable {
        doTestDismissContextMenuOnClick(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testDismissContextMenuOnClick_popup() throws Throwable {
        doTestDismissContextMenuOnClick(true);
    }

    private void doTestDismissContextMenuOnClick(boolean isPopup) throws Exception {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE, true, "", "http://www.test_link.html/", "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links", mCoordinator);

        AnchoredPopupWindow popupWindow = mCoordinator.getPopupWindowForTesting();
        ComponentDialog dialog = mCoordinator.getDialogForTesting();

        if (isPopup) {
            Assert.assertNotNull("Popup menu should be created for links", popupWindow);
            Assert.assertTrue(popupWindow.isShowing());
        } else {
            Assert.assertNotNull("Dialog menu should be created for links", dialog);
            Assert.assertTrue(dialog.isShowing());
            CriteriaHelper.pollUiThread(
                    () -> {
                        return !mRule.getActivity().hasWindowFocus();
                    },
                    "Context menu should have window focus");
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.clickListItemForTesting(R.id.contextmenu_copy_link_text));

        CriteriaHelper.pollUiThread(
                () -> {
                    return isPopup
                            ? mCoordinator.getPopupWindowForTesting() == null
                            : mRule.getActivity().hasWindowFocus();
                },
                "Activity should have regained focus.");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBuildingContextMenuItems() throws Throwable {
        Integer[] expectedItems = {
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_open_link_id,
        };

        ContextMenuParams params =
                createContextMenuParams(ContextMenuDataMediaType.NONE, true, "BLAH!", "", "");

        AwContextMenuPopulator populator =
                new AwContextMenuPopulator(
                        mContext,
                        mRule.getActivity(),
                        mAwContents.getWebContents(),
                        params,
                        /* usePopupWindow= */ false);

        List<ModelList> contextMenuState = populator.buildContextMenu();

        ModelList items = contextMenuState.get(0);
        Integer[] actualItems = new Integer[items.size()];

        for (int i = 0; i < items.size(); i++) {
            actualItems[i] = populator.getMenuId(items.get(i).model);
        }

        Assert.assertArrayEquals(actualItems, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHeaderHasURLText() throws Throwable {
        String expectedHeaderText = "http://www.testurl.com/first_page";

        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE, true, "BLAH!", expectedHeaderText, "");

        AwContextMenuHeaderCoordinator headerCoordinator =
                new AwContextMenuHeaderCoordinator(params, mContext);

        String actualHeaderTitle = headerCoordinator.getTitle();

        Assert.assertEquals(expectedHeaderText, actualHeaderTitle);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testContextMenuNotDisplayedForImages_dialog() throws Throwable {
        doTestContextMenuNotDisplayedForImages(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testContextMenuNotDisplayedForImages_popup() throws Throwable {
        doTestContextMenuNotDisplayedForImages(true);
    }

    public void doTestContextMenuNotDisplayedForImages(boolean isPopup) throws Throwable {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.IMAGE,
                        false,
                        "",
                        "",
                        "http://www.image_source.html/");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        // Check the context menu is not displayed.
        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNull("Context menu should not be created for images", mCoordinator);

        // On an anchor link with a nested image, the menu should be shown since this is
        // technically a link.
        ContextMenuParams params2 =
                createContextMenuParams(
                        ContextMenuDataMediaType.IMAGE,
                        true,
                        "Test Link Image",
                        "http://www.test_link.html/",
                        "http://www.image_source.html/");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params2, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Context menu should be created for links with images", mCoordinator);

        if (isPopup) {
            Assert.assertNotNull(
                    "Popup menu should be created for links with images",
                    mCoordinator.getPopupWindowForTesting());
            Assert.assertTrue(mCoordinator.getPopupWindowForTesting().isShowing());
        } else {
            Assert.assertNotNull(
                    "Dialog menu should be created for links with images",
                    mCoordinator.getDialogForTesting());
            Assert.assertTrue(mCoordinator.getDialogForTesting().isShowing());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testContextMenuNotDisplayedForVideos_dialog() throws Throwable {
        doTestContextMenuNotDisplayedForVideos(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testContextMenuNotDisplayedForVideos_popup() throws Throwable {
        doTestContextMenuNotDisplayedForVideos(true);
    }

    public void doTestContextMenuNotDisplayedForVideos(boolean isPopup) {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.VIDEO,
                        false,
                        "Test Link Video",
                        "http://www.test_link.html/",
                        "http://www.image_source.html/");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNull("Context menu should not be created for videos", mCoordinator);
    }

    @Test
    @MediumTest
    @Feature("AndroidWebView")
    public void doNotShowContextMenuForNonLinkItems() {
        final ContextMenuParams params =
                new ContextMenuParams(
                        /* nativePtr= */ 0,
                        new MenuModelBridge(),
                        ContextMenuDataMediaType.NONE,
                        /* pageUrl= */ GURL.emptyGURL(),
                        /* linkUrl= */ GURL.emptyGURL(),
                        /* linkText= */ "Test link text",
                        /* unfilteredLinkUrl= */ GURL.emptyGURL(),
                        /* srcUrl= */ GURL.emptyGURL(),
                        /* titleText= */ "Test title",
                        /* referrer= */ null,
                        /* canSaveMedia= */ false,
                        /* triggeringTouchXDp= */ 0,
                        /* triggeringTouchYDp= */ 0,
                        /* sourceType= */ 0,
                        /* openedFromHighlight= */ false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        AwContextMenuHelper helper = AwContextMenuHelper.create(mAwContents.getWebContents());
        Assert.assertFalse(helper.showContextMenu(params, mTestContainerView));
    }

    private ContextMenuParams createContextMenuParams(
            @ContextMenuDataMediaType int mediaType,
            Boolean linkUrl,
            String linkText,
            String unfilteredLinkUrl,
            String srcUrl) {

        return new ContextMenuParams(
                0,
                new MenuModelBridge(),
                mediaType,
                new GURL("http://www.example.com/page_url"),
                linkUrl ? new GURL("http://www.example.com/other_example") : GURL.emptyGURL(),
                linkText,
                new GURL(unfilteredLinkUrl),
                new GURL(srcUrl),
                "",
                null,
                false,
                0,
                0,
                MenuSourceType.TOUCH,
                false,
                /* openedFromInterestFor= */ false,
                /* interestForNodeID= */ 0,
                /* additionalNavigationParams= */ null);
    }
}
