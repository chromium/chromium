// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;

import static org.hamcrest.Matchers.equalTo;

import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.COPY_LINK_ADDRESS;
import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.COPY_LINK_TEXT;
import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.DISABLED;
import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.OPEN_LINK;
import static org.chromium.android_webview.contextmenu.AwContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.util.Pair;

import androidx.activity.ComponentDialog;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
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
import org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems;
import org.chromium.android_webview.R;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.contextmenu.AwContextMenuCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHeaderCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHeaderProperties;
import org.chromium.android_webview.contextmenu.AwContextMenuHelper;
import org.chromium.android_webview.contextmenu.AwContextMenuItemProperties;
import org.chromium.android_webview.contextmenu.AwContextMenuPopulator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuSwitches;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for context menu methods */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU})
public class ContextMenuTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContents mAwContents;
    private Context mContext;
    private AwContextMenuHelper mHelper;
    private AwContextMenuCoordinator mCoordinator;
    private GURL mPageUrl;
    private static final @HyperlinkContextMenuItems int HYPERLINK_MENU_ITEMS =
            COPY_LINK_ADDRESS | COPY_LINK_TEXT | OPEN_LINK;

    public ContextMenuTest(AwSettingsMutation param) {
        mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mPageUrl = new GURL("http://www.example.com/page_url");
        TestAwContentsClient mContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestAwContentsClientTestDependencyFactory());

        mAwContents = (TestAwContents) mTestContainerView.getAwContents();
        mContext = mAwContents.getWebContents().getTopLevelNativeWindow().getContext().get();
        mHelper = new TestAwContextMenuHelper(mAwContents.getWebContents(), HYPERLINK_MENU_ITEMS);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() {
        mHelper = null;
        mCoordinator = null;
        AwContextMenuHeaderCoordinator.setCachedFaviconForTesting(null);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCopyLinkText() throws Throwable {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link",
                        /* unfilteredLinkUrl= */ "",
                        /* srcUrl= */ "");

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
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link Text",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");
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
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link Image",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "http://www.image_source.html/");
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
                            /* linkUrl= */ true,
                            /* linkText= */ "",
                            /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                            /* srcUrl= */ "");
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
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");
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
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");
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
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link Text",
                        /* unfilteredLinkUrl= */ "",
                        /* srcUrl= */ "");

        AwContextMenuPopulator populator =
                new AwContextMenuPopulator(
                        mContext,
                        mRule.getActivity(),
                        mAwContents.getWebContents(),
                        params,
                        /* usePopupWindow= */ false,
                        /* menuItems= */ HYPERLINK_MENU_ITEMS);

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
    public void testCorrectMenuItemsShown_disabled() throws Throwable {
        doTestCorrectMenuItemsShown(DISABLED, new ArrayList<>());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_linkAddress() throws Throwable {
        doTestCorrectMenuItemsShown(
                COPY_LINK_ADDRESS, Arrays.asList(R.id.contextmenu_copy_link_address));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_linkText() throws Throwable {
        doTestCorrectMenuItemsShown(COPY_LINK_TEXT, Arrays.asList(R.id.contextmenu_copy_link_text));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_openLink() throws Throwable {
        doTestCorrectMenuItemsShown(OPEN_LINK, Arrays.asList(R.id.contextmenu_open_link_id));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_linkAddressAndLinkText() throws Throwable {
        doTestCorrectMenuItemsShown(
                COPY_LINK_ADDRESS | COPY_LINK_TEXT,
                Arrays.asList(R.id.contextmenu_copy_link_address, R.id.contextmenu_copy_link_text));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_linkAddressAndOpenLink() throws Throwable {
        doTestCorrectMenuItemsShown(
                COPY_LINK_ADDRESS | OPEN_LINK,
                Arrays.asList(R.id.contextmenu_copy_link_address, R.id.contextmenu_open_link_id));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_linkTextAndOpenLink() throws Throwable {
        doTestCorrectMenuItemsShown(
                COPY_LINK_TEXT | OPEN_LINK,
                Arrays.asList(R.id.contextmenu_copy_link_text, R.id.contextmenu_open_link_id));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorrectMenuItemsShown_All() throws Throwable {
        doTestCorrectMenuItemsShown(
                COPY_LINK_ADDRESS | COPY_LINK_TEXT | OPEN_LINK,
                Arrays.asList(
                        R.id.contextmenu_copy_link_address,
                        R.id.contextmenu_copy_link_text,
                        R.id.contextmenu_open_link_id));
    }

    private void doTestCorrectMenuItemsShown(
            @HyperlinkContextMenuItems Integer menuItems, List<Integer> expectedItems)
            throws Exception {
        mHelper = new TestAwContextMenuHelper(mAwContents.getWebContents(), menuItems);

        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.showContextMenu(params, mTestContainerView));

        mCoordinator = mHelper.getCoordinatorForTesting();
        Assert.assertNotNull("Coordinator should be created for links", mCoordinator);

        if (menuItems == DISABLED) {
            Assert.assertTrue(
                    "Context menu should not be shown if there are no items",
                    mCoordinator.getDialogForTesting() == null
                            && mCoordinator.getPopupWindowForTesting() == null);
        } else {
            assertMenuItemsAreEqual(mCoordinator, expectedItems);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHeaderHasURLText() throws Throwable {
        String expectedHeaderText = "http://www.testurl.com/first_page";

        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link Text",
                        /* unfilteredLinkUrl= */ expectedHeaderText,
                        /* srcUrl= */ "");

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
                        /* linkUrl= */ false,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "",
                        /* srcUrl= */ "http://www.image_source.html/");
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
                        /* linkUrl= */ true,
                        /* linkText= */ "Test Link Image",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "http://www.image_source.html/");
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
                        /* linkUrl= */ false,
                        /* linkText= */ "Test Link Video",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "http://www.image_source.html/");
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
                        new MenuModelBridge(0L),
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        /* pageUrl= */ GURL.emptyGURL(),
                        /* linkUrl= */ GURL.emptyGURL(),
                        /* linkText= */ "Test Link Text",
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

        Assert.assertFalse(mHelper.showContextMenu(params, mTestContainerView));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testInitialHeaderIconSet() throws Throwable {
        Bitmap bitmap = createTestBitmap(3, 3, Color.RED);
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");

        Assert.assertNull(
                "Cache should be empty before context menu shown",
                AwContextMenuHeaderCoordinator.getCachedFaviconForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap);
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        Pair<String, Drawable> cached = AwContextMenuHeaderCoordinator.getCachedFaviconForTesting();
        BitmapDrawable cachedBitmap = (BitmapDrawable) cached.second;

        Assert.assertNotNull("Cache should be populated after first icon set", cached);
        Assert.assertEquals("Host should match", mPageUrl.getHost(), cached.first);
        Assert.assertSame(
                "Drawable should wrap the original bitmap", cachedBitmap.getBitmap(), bitmap);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testHeaderIcon_sameHostSameIcon() throws Throwable {
        Bitmap bitmap = createTestBitmap(5, 5, Color.BLUE);
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap);
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header1 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        BitmapDrawable firstDrawable =
                (BitmapDrawable) header1.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);
        Assert.assertNotNull("Drawable icon should have been set", firstDrawable);

        // Second trigger with same host and bitmap.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        BitmapDrawable secondDrawable =
                (BitmapDrawable) AwContextMenuHeaderCoordinator.getCachedFaviconForTesting().second;
        Assert.assertSame(
                "Cached drawable should be used if host and icon match",
                firstDrawable.getBitmap(),
                secondDrawable.getBitmap());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testHeaderIcon_differentHostSameIcon() throws Throwable {
        Bitmap bitmap = createTestBitmap(7, 7, Color.GREEN);
        ContextMenuParams params1 =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");

        // First context menu with host1.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap);
                    mHelper.showContextMenu(params1, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header1 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        Drawable firstDrawable = header1.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        // Second context menu with host2 and same bitmap.
        mPageUrl = new GURL("http://host2.com/page"); // update url for second call.
        ContextMenuParams params2 =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHelper.showContextMenu(params2, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header2 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        VectorDrawable secondDrawable =
                (VectorDrawable) header2.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        Drawable expectedFallback = ContextCompat.getDrawable(mContext, R.drawable.ic_globe_24dp);
        Assert.assertNotNull("Fallback drawable could not be loaded", expectedFallback);

        expectedFallback = DrawableCompat.wrap(expectedFallback.mutate());
        DrawableCompat.setTint(
                expectedFallback,
                ContextCompat.getColor(mContext, R.color.default_icon_color_baseline));

        // Ensure correct fallback icon is used.
        Bitmap expectedBitmap = drawableToBitmap(expectedFallback);
        Bitmap actualBitmap = drawableToBitmap(secondDrawable);
        assertBitmapsEqual(expectedBitmap, actualBitmap);

        Assert.assertNotSame(
                "Fallback icon should be used if host and icon don't match",
                firstDrawable,
                secondDrawable);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testHeaderIcon_nullIcon() throws Throwable {
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(null);
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        Drawable drawable = header.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        Assert.assertNotNull("Fallback icon should be used if icon is null", drawable);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testHeaderIcon_differentHostDifferentIcon() throws Throwable {
        Bitmap bitmap1 = createTestBitmap(9, 9, Color.YELLOW);
        ContextMenuParams params1 =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE,
                        /* linkUrl= */ true,
                        /* linkText= */ "",
                        /* unfilteredLinkUrl= */ "http://www.test_link.html/",
                        /* srcUrl= */ "");

        // First call with host1.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap1);
                    mHelper.showContextMenu(params1, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header1 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        BitmapDrawable firstDrawable =
                (BitmapDrawable) header1.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        // Second call with host2 and different bitmap.
        Bitmap bitmap2 = createTestBitmap(11, 11, Color.MAGENTA);
        mPageUrl = new GURL("http://host2.com/page"); // update url for second call.
        ContextMenuParams params2 =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE, true, "", "http://www.test_link.html/", "");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap2);
                    mHelper.showContextMenu(params2, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header2 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        BitmapDrawable secondDrawable =
                (BitmapDrawable) header2.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        Assert.assertNotSame(
                "Drawable should update for different host and icon",
                firstDrawable.getBitmap(),
                secondDrawable.getBitmap());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testHeaderIcon_sameHostDifferentIcon() throws Throwable {
        Bitmap bitmap1 = createTestBitmap(13, 13, Color.CYAN);
        ContextMenuParams params =
                createContextMenuParams(
                        ContextMenuDataMediaType.NONE, true, "", "http://www.test_link.html/", "");

        // First icon set.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap1);
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header1 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        BitmapDrawable firstDrawable =
                (BitmapDrawable) header1.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        // Second icon set with different bitmap.
        Bitmap bitmap2 = createTestBitmap(15, 15, Color.GRAY);
        bitmap2.setPixel(0, 0, 0xFFFF0000); // Force different content.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.setFaviconForTesting(bitmap2);
                    mHelper.showContextMenu(params, mTestContainerView);
                });

        AwContextMenuHeaderCoordinator header2 =
                mHelper.getCoordinatorForTesting().getHeaderCoordinatorForTesting();
        BitmapDrawable secondDrawable =
                (BitmapDrawable) header2.getModel().get(AwContextMenuHeaderProperties.HEADER_ICON);

        Assert.assertNotSame(
                "Drawable should update for same host but different icon",
                firstDrawable.getBitmap(),
                secondDrawable.getBitmap());
    }

    private ContextMenuParams createContextMenuParams(
            @ContextMenuDataMediaType int mediaType,
            boolean linkUrl,
            String linkText,
            String unfilteredLinkUrl,
            String srcUrl) {

        return new ContextMenuParams(
                0,
                new MenuModelBridge(0L),
                mediaType,
                ContextMenuDataMediaFlags.MEDIA_NONE,
                mPageUrl,
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

    private Bitmap createTestBitmap(int width, int height, int color) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(color);
        return bitmap;
    }

    private Bitmap drawableToBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    private void assertBitmapsEqual(Bitmap expected, Bitmap actual) {
        Assert.assertEquals("Widths do not match", expected.getWidth(), actual.getWidth());
        Assert.assertEquals("Heights do not match", expected.getHeight(), actual.getHeight());

        for (int x = 0; x < expected.getWidth(); x++) {
            for (int y = 0; y < expected.getHeight(); y++) {
                int expectedPixel = expected.getPixel(x, y);
                int actualPixel = actual.getPixel(x, y);
                Assert.assertEquals(
                        "Pixels differ at (" + x + "," + y + ")", expectedPixel, actualPixel);
            }
        }
    }

    /**
     * Takes all the visible items on the menu and compares them to a the list of expected items.
     *
     * @param menu A context menu that is displaying visible items.
     * @param expectedItems A list of items that is expected to appear within a context menu.
     */
    private void assertMenuItemsAreEqual(
            AwContextMenuCoordinator menu, List<Integer> expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < getCount(menu); i++) {
            if (getItem(menu, i).type >= CONTEXT_MENU_ITEM) {
                actualItems.add(getItem(menu, i).model.get(AwContextMenuItemProperties.MENU_ID));
            }
        }

        Assert.assertEquals(
                "Populated menu items were:" + getMenuTitles(menu), expectedItems, actualItems);
    }

    private String getMenuTitles(AwContextMenuCoordinator menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < getCount(menu); i++) {
            if (getItem(menu, i).type >= CONTEXT_MENU_ITEM) {
                items.append("\n")
                        .append(getItem(menu, i).model.get(AwContextMenuItemProperties.TEXT));
            }
        }
        return items.toString();
    }

    private ListItem getItem(AwContextMenuCoordinator menu, int index) {
        return (ListItem) menu.getListViewForTest().getAdapter().getItem(index);
    }

    private int getCount(AwContextMenuCoordinator menu) {
        return menu.getListViewForTest().getAdapter().getCount();
    }

    private static class TestAwContextMenuHelper extends AwContextMenuHelper {
        private final @HyperlinkContextMenuItems int mHyperlinkMenuItems;

        public TestAwContextMenuHelper(
                WebContents webContents, @HyperlinkContextMenuItems int hyperlinkMenuItems) {
            super(webContents);
            mHyperlinkMenuItems = hyperlinkMenuItems;
        }

        @Override
        protected @HyperlinkContextMenuItems int getHyperlinkContextMenuItems(
                AwContents awContents) {
            return mHyperlinkMenuItems;
        }
    }
}
