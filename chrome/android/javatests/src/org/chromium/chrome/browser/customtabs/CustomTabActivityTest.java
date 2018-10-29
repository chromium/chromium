// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.DrawableRes;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSession;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RemoteViews;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabsOpenedFromExternalAppTest;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentUtils;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabActivityTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final int TIMEOUT_PAGE_LOAD_SECONDS = 10;
    private static final int MAX_MENU_CUSTOM_ITEMS = 5;
    private static final int NUM_CHROME_MENU_ITEMS = 5;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String SELECT_POPUP_PAGE = "/chrome/test/data/android/select.html";
    private static final String FRAGMENT_TEST_PAGE = "/chrome/test/data/android/fragment.html";
    private static final String TEST_MENU_TITLE = "testMenuTitle";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final String WEBLITE_PREFIX = "http://googleweblight.com/i?u=";
    private static final String JS_MESSAGE = "from_js";
    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received = '';"
            + "        onmessage = function (e) {"
            + "            var myport = e.ports[0];"
            + "            myport.onmessage = function (f) {"
            + "                received += f.data;"
            + "                document.title = received;"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String MESSAGE_FROM_PAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            if (e.ports != null && e.ports.length > 0) {"
            + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String ONLOAD_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "            document.title = \"nytimes.com\";"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String DELAYED_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "           setTimeout(function (){ document.title = \"nytimes.com\"}, 200);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private static int sIdToIncrement = 1;

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;
    private TestWebServer mWebServer;

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        mTestServer.stopAndDestroyServer();

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (getActivity() == null) return;
            AppMenuHandler handler = getActivity().getAppMenuHandler();
            if (handler != null) handler.hideAppMenu();
        });
        mWebServer.shutdown();
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
    }

    /**
     * Add a bundle specifying a a number of custom menu entries.
     * @param customTabIntent The intent to modify.
     * @param numEntries The number of menu entries to add.
     * @return The pending intent associated with the menu entries.
     */
    private PendingIntent addMenuEntriesToIntent(Intent customTabIntent, int numEntries) {
        return addMenuEntriesToIntent(customTabIntent, numEntries, new Intent());
    }

    /**
     * Add a bundle specifying a custom menu entry.
     * @param customTabIntent The intent to modify.
     * @param numEntries The number of menu entries to add.
     * @param callbackIntent The intent to use as the base for the pending intent.
     * @return The pending intent associated with the menu entry.
     */
    private PendingIntent addMenuEntriesToIntent(
            Intent customTabIntent, int numEntries, Intent callbackIntent) {
        PendingIntent pi = PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(), 0,
                callbackIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        ArrayList<Bundle> menuItems = new ArrayList<>();
        for (int i = 0; i < numEntries; i++) {
            Bundle bundle = new Bundle();
            bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, TEST_MENU_TITLE);
            bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
            menuItems.add(bundle);
        }
        customTabIntent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS, menuItems);
        return pi;
    }

    private void addToolbarColorToIntent(Intent intent, int color) {
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, color);
    }

    /**
     * Adds an action button to the custom tab toolbar.
     * @return The {@link PendingIntent} that will be triggered when the action button is clicked.
     */
    private PendingIntent addActionButtonToIntent(Intent intent, Bitmap icon, String description) {
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                makeToolbarItemBundle(icon, description, pi));
        return pi;
    }

    private Bundle makeToolbarItemBundle(Bitmap icon, String description, PendingIntent pi) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, sIdToIncrement++);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putBoolean(CustomButtonParams.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    private Bundle makeBottomBarBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);

        bundle.putInt(CustomTabsIntent.KEY_ID, sIdToIncrement++);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        return bundle;
    }

    private Bundle makeUpdateVisualsBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        return bundle;
    }

    private void openAppMenuAndAssertMenuShown() {
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mCustomTabActivityTestRule.getActivity());
    }

    /**
     * @return The number of visible and enabled items in the given menu.
     */
    private int getActualMenuSize(Menu menu) {
        int actualMenuSize = 0;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible() && item.isEnabled()) actualMenuSize++;
        }
        return actualMenuSize;
    }

    /**
     * @return The number of visible items in the given menu.
     */
    private int getVisibleMenuSize(Menu menu) {
        int visibleMenuSize = 0;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible()) visibleMenuSize++;
        }
        return visibleMenuSize;
    }

    private Bitmap createTestBitmap(int widthDp, int heightDp) {
        Resources testRes = InstrumentationRegistry.getTargetContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        return Bitmap.createBitmap((int) (widthDp * density),
                (int) (heightDp * density), Bitmap.Config.ARGB_8888);
    }

    private Bitmap createVectorDrawableBitmap(@DrawableRes int resId, int widthDp, int heightDp) {
        Context context = InstrumentationRegistry.getTargetContext();
        Drawable vectorDrawable = AppCompatResources.getDrawable(context, resId);
        Bitmap bitmap = createTestBitmap(widthDp, heightDp);
        Canvas canvas = new Canvas(bitmap);
        float density = context.getResources().getDisplayMetrics().density;
        int widthPx = (int) (density * widthDp);
        int heightPx = (int) (density * heightDp);
        vectorDrawable.setBounds(0, 0, widthPx, heightPx);
        vectorDrawable.draw(canvas);
        return bitmap;
    }

    private static boolean isSelectPopupVisible(ChromeActivity activity) {
        Tab tab = activity.getActivityTab();
        if (tab == null || tab.getWebContents() == null) return false;
        return WebContentsUtils.isSelectPopupVisible(tab.getWebContents());
    }

    /**
     * Test the entries in the context menu shown when long clicking an image.
     * @SmallTest
     * @RetryOnFailure
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForImage() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "logo");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_search_by_image).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking a link.
     * @SmallTest
     * @RetryOnFailure
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForLink() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "aboutLink");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_save_link_as).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an mailto url.
     * @SmallTest
     * @RetryOnFailure
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForMailto() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "email");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an tel url.
     * @SmallTest
     * @RetryOnFailure
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForTel() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "tel");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the app menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenu() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 1;
        addMenuEntriesToIntent(intent, numMenuEntries);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
        Assert.assertNotNull(menu.findItem(R.id.forward_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.bookmark_this_page_id));
        Assert.assertNotNull(menu.findItem(R.id.offline_page_id));
        Assert.assertNotNull(menu.findItem(R.id.info_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.reload_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.open_in_browser_id));
        Assert.assertFalse(menu.findItem(R.id.share_row_menu_id).isVisible());
        Assert.assertFalse(menu.findItem(R.id.share_row_menu_id).isEnabled());
        Assert.assertNotNull(menu.findItem(R.id.find_in_page_id));
        Assert.assertNotNull(menu.findItem(R.id.add_to_homescreen_id));
        Assert.assertNotNull(menu.findItem(R.id.request_desktop_site_row_menu_id));
    }

    /**
     * Test the entries in app menu for media viewer.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForMediaViewer() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabIntentDataProvider.CustomTabsUiType.MEDIA_VIEWER);
        IntentHandler.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        final int expectedMenuSize = 0;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
    }

    /**
     * Test the entries in app menu for Reader Mode.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForReaderMode() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabIntentDataProvider.CustomTabsUiType.READER_MODE);
        IntentHandler.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        final int expectedMenuSize = 2;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
        Assert.assertTrue(menu.findItem(R.id.find_in_page_id).isVisible());
        Assert.assertTrue(menu.findItem(R.id.reader_mode_prefs_id).isVisible());
    }

    /**
     * Test the entries in app menu for media viewer.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForOfflinePage() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabIntentDataProvider.CustomTabsUiType.OFFLINE_PAGE);
        IntentHandler.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        final int expectedMenuSize = 3;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
        Assert.assertTrue(menu.findItem(R.id.find_in_page_id).isVisible());
        Assert.assertNotNull(menu.findItem(R.id.request_desktop_site_row_menu_id));

        MenuItem icon_row = menu.findItem(R.id.icon_row_menu_id);
        Assert.assertNotNull(icon_row);
        Assert.assertNotNull(icon_row.hasSubMenu());
        SubMenu icon_row_menu = icon_row.getSubMenu();
        final int expectedIconMenuSize = 4;
        Assert.assertEquals(expectedIconMenuSize, getVisibleMenuSize(icon_row_menu));
        Assert.assertNotNull(icon_row_menu.findItem(R.id.forward_menu_id));
        Assert.assertNotNull(icon_row_menu.findItem(R.id.bookmark_this_page_id));
        Assert.assertNotNull(icon_row_menu.findItem(R.id.info_menu_id));
        Assert.assertNotNull(icon_row_menu.findItem(R.id.reload_menu_id));
    }

    /**
     * Tests if the default share item can be shown in the app menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testShareMenuItem() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        Assert.assertTrue(menu.findItem(R.id.share_menu_id).isVisible());
        Assert.assertTrue(menu.findItem(R.id.share_menu_id).isEnabled());
    }


    /**
     * Test that only up to 5 entries are added to the custom menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMaxMenuItems() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 7;
        Assert.assertTrue(MAX_MENU_CUSTOM_ITEMS < numMenuEntries);
        addMenuEntriesToIntent(intent, numMenuEntries);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        final int expectedMenuSize = MAX_MENU_CUSTOM_ITEMS + NUM_CHROME_MENU_ITEMS;
        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
    }

    /**
     * Test whether the custom menu is correctly shown and clicking it sends the right
     * {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCustomMenuEntry() throws InterruptedException, TimeoutException {
        Intent customTabIntent = createMinimalCustomTabIntent();
        Intent baseCallbackIntent = new Intent();
        baseCallbackIntent.putExtra("FOO", 42);
        final PendingIntent pi = addMenuEntriesToIntent(customTabIntent, 1, baseCallbackIntent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity().getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(() -> {
            MenuItem item = getActivity().getAppMenuPropertiesDelegate().getMenuItemForTitle(
                    TEST_MENU_TITLE);
            Assert.assertNotNull(item);
            Assert.assertTrue(getActivity().onOptionsItemSelected(item));
        });

        onFinished.waitForCallback("Pending Intent was not sent.");
        Intent callbackIntent = onFinished.getCallbackIntent();
        Assert.assertThat(callbackIntent.getDataString(), equalTo(mTestPage));

        // Verify that the callback intent has the page title as the subject, but other extras are
        // kept intact.
        Assert.assertThat(
                callbackIntent.getStringExtra(Intent.EXTRA_SUBJECT), equalTo("The Google"));
        Assert.assertThat(callbackIntent.getIntExtra("FOO", 0), equalTo(42));
    }

    /**
     * Test whether clicking "Open in Chrome" takes us to a chrome normal tab, loading the same url.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testOpenInBrowser() throws Exception {
        // Augment the CustomTabsSession to catch the callback.
        CallbackHelper callbackTriggered = new CallbackHelper();
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.OPEN_IN_BROWSER_CALLBACK)) {
                    callbackTriggered.notifyCalled();
                }
            }
        });

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(Uri.parse(mTestServer.getURL("/")).getScheme());
        final ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, false);
        openAppMenuAndAssertMenuShown();
        final String menuItemTitle = getActivity().getString(R.string.menu_open_in_product_default);
        ThreadUtils.runOnUiThread(() -> {
            MenuItem item = getActivity().getAppMenuHandler().getAppMenu().getMenu().findItem(
                    R.id.open_in_browser_id);
            Assert.assertNotNull(item);
            Assert.assertEquals(menuItemTitle, item.getTitle().toString());
            getActivity().onMenuOrKeyboardAction(R.id.open_in_browser_id, false);
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return InstrumentationRegistry.getInstrumentation().checkMonitorHit(monitor, 1);
            }
        });

        callbackTriggered.waitForCallback(0);
    }

    /**
     * Test whether a custom tab can be reparented to a new activity.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testTabReparentingBasic() throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        reparentAndVerifyTab();
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing an infobar.
     *
     * TODO(timloh): Use a different InfoBar type once we only use modals for permission prompts.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS)
    @RetryOnFailure
    public void testTabReparentingInfoBar() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(GEOLOCATION_PAGE)));
        CriteriaHelper.pollUiThread(
                () -> isInfoBarSizeOne(mCustomTabActivityTestRule.getActivity().getActivityTab()));

        ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(() -> isInfoBarSizeOne(newActivity.getActivityTab()));
    }

    private static boolean isInfoBarSizeOne(Tab tab) {
        if (tab == null) return false;
        InfoBarContainer container = InfoBarContainer.get(tab);
        if (container == null) return false;
        return container.getInfoBarsForTesting().size() == 1;
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing a select popup.
     */
    // @SmallTest
    // @RetryOnFailure
    @Test
    @DisabledTest // Disabled due to flakiness on browser_side_navigation apk - see crbug.com/707766
    public void testTabReparentingSelectPopup() throws InterruptedException, TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(SELECT_POPUP_PAGE)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return currentTab != null && currentTab.getWebContents() != null;
            }
        });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "select");
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isSelectPopupVisible(mCustomTabActivityTestRule.getActivity());
            }
        });
        final ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isSelectPopupVisible(newActivity);
            }
        });
    }
    /**
     * Test whether the color of the toolbar is correctly customized. For L or later releases,
     * status bar color is also tested.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testToolbarColor() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        Assert.assertEquals(expectedColor, toolbar.getBackground().getColor());
        Assert.assertFalse(mCustomTabActivityTestRule.getActivity()
                                   .getToolbarManager()
                                   .getToolbarModelForTesting()
                                   .shouldEmphasizeHttpsScheme());
        // TODO(https://crbug.com/871805): Use helper class to determine whether dark status icons
        // are supported.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Assert.assertEquals(expectedColor,
                    mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        } else if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            Assert.assertEquals(ColorUtils.getDarkenedColorForStatusBar(expectedColor),
                    mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        }
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @RetryOnFailure
    public void testActionButton() throws InterruptedException, TimeoutException {
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addActionButtonToIntent(intent, expectedIcon, "Good test");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity().getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        Assert.assertNotNull(actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue("Action button does not have the correct bitmap.",
                expectedIcon.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Action Buttons");

        ThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished.waitForCallback("Pending Intent was not sent.");
        Assert.assertThat(onFinished.getCallbackIntent().getDataString(), equalTo(mTestPage));
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @RetryOnFailure
    public void testMultipleActionButtons() throws InterruptedException, TimeoutException {
        Bitmap expectedIcon1 = createVectorDrawableBitmap(R.drawable.ic_content_copy_black, 48, 48);
        Bitmap expectedIcon2 = createVectorDrawableBitmap(R.drawable.ic_music_note_36dp, 48, 48);
        Intent intent = createMinimalCustomTabIntent();

        // Mark the intent as trusted so it can show more than one action button.
        IntentHandler.addTrustedIntentExtras(intent);
        Assert.assertTrue(IntentHandler.notSecureIsIntentChromeOrFirstParty(intent));

        ArrayList<Bundle> toolbarItems = new ArrayList<>(2);
        final PendingIntent pi1 = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        final OnFinishedForTest onFinished1 = new OnFinishedForTest(pi1);
        toolbarItems.add(makeToolbarItemBundle(expectedIcon1, "Good test", pi1));
        final PendingIntent pi2 = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 1, new Intent(), 0);
        Assert.assertThat(pi2, not(equalTo(pi1)));
        final OnFinishedForTest onFinished2 = new OnFinishedForTest(pi2);
        toolbarItems.add(makeToolbarItemBundle(expectedIcon2, "Even gooder test", pi2));
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, toolbarItems);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Forward the onFinished event to both objects.
        getActivity().getIntentDataProvider().setPendingIntentOnFinishedForTesting(
                (pendingIntent, openedIntent, resultCode, resultData, resultExtras) -> {
                    onFinished1.onSendFinished(
                            pendingIntent, openedIntent, resultCode, resultData, resultExtras);
                    onFinished2.onSendFinished(
                            pendingIntent, openedIntent, resultCode, resultData, resultExtras);
                });

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(1);

        Assert.assertNotNull("Action button not found", actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue("Action button does not have the correct bitmap.",
                expectedIcon1.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Multiple Action Buttons");

        ThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished1.waitForCallback("Pending Intent was not sent.");
        Assert.assertThat(onFinished1.getCallbackIntent().getDataString(), equalTo(mTestPage));
        Assert.assertNull(onFinished2.getCallbackIntent());

        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        int id = toolbarItems.get(0).getInt(CustomTabsIntent.KEY_ID);
        Bundle updateActionButtonBundle =
                makeUpdateVisualsBundle(id, expectedIcon2, "Bestest testest");
        Bundle updateVisualsBundle = new Bundle();
        updateVisualsBundle.putParcelableArrayList(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS,
                new ArrayList<>(Arrays.asList(updateActionButtonBundle)));
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.updateVisuals(token, updateVisualsBundle));

        Assert.assertEquals("Bestest testest", actionButton.getContentDescription());
    }

    /**
     * Test that additional action buttons are ignored for untrusted intents.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @RetryOnFailure
    public void testMultipleActionButtons_untrusted()
            throws InterruptedException, TimeoutException {
        Bitmap expectedIcon1 = createVectorDrawableBitmap(R.drawable.ic_content_copy_black, 48, 48);
        Bitmap expectedIcon2 = createVectorDrawableBitmap(R.drawable.ic_music_note_36dp, 48, 48);
        Intent intent = createMinimalCustomTabIntent();

        // By default, the intent should not be trusted.
        Assert.assertFalse(IntentHandler.notSecureIsIntentChromeOrFirstParty(intent));

        ArrayList<Bundle> toolbarItems = new ArrayList<>(2);
        final PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        toolbarItems.add(makeToolbarItemBundle(expectedIcon1, "Shown", pi));
        toolbarItems.add(makeToolbarItemBundle(expectedIcon2, "Not shown", pi));
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, toolbarItems);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);
        Assert.assertNotNull("Action button not found", actionButton);
        Assert.assertEquals("Shown", actionButton.getContentDescription());

        Assert.assertNull(toolbar.getCustomActionButtonForTest(1));
    }

    /**
     * Test the case that the action button should not be shown, given a bitmap with unacceptable
     * height/width ratio.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testActionButtonBadRatio() throws InterruptedException {
        Bitmap expectedIcon = createTestBitmap(60, 20);
        Intent intent = createMinimalCustomTabIntent();
        addActionButtonToIntent(intent, expectedIcon, "Good test");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        Assert.assertNull("Action button should not be shown", actionButton);

        CustomTabIntentDataProvider dataProvider = getActivity().getIntentDataProvider();
        Assert.assertThat(dataProvider.getCustomButtonsOnToolbar(), is(empty()));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testBottomBar() throws InterruptedException {
        final int numItems = 3;
        final Bitmap expectedIcon = createTestBitmap(48, 24);
        final int barColor = Color.GREEN;

        Intent intent = createMinimalCustomTabIntent();
        ArrayList<Bundle> bundles = new ArrayList<>();
        for (int i = 1; i <= numItems; i++) {
            Bundle bundle = makeBottomBarBundle(i, expectedIcon, Integer.toString(i));
            bundles.add(bundle);
        }
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, bundles);
        intent.putExtra(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR, barColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ViewGroup bottomBar = mCustomTabActivityTestRule.getActivity().findViewById(
                R.id.custom_tab_bottom_bar_wrapper);
        Assert.assertNotNull(bottomBar);
        Assert.assertTrue("Bottom Bar wrapper is not visible.",
                bottomBar.getVisibility() == View.VISIBLE && bottomBar.getHeight() > 0
                        && bottomBar.getWidth() > 0);
        Assert.assertEquals("Bottom Bar showing incorrect number of buttons.", numItems,
                bottomBar.getChildCount());
        Assert.assertEquals("Bottom bar not showing correct color", barColor,
                ((ColorDrawable) bottomBar.getBackground()).getColor());
        for (int i = 0; i < numItems; i++) {
            ImageButton button = (ImageButton) bottomBar.getChildAt(i);
            Assert.assertTrue("Bottom Bar button does not have the correct bitmap.",
                    expectedIcon.sameAs(((BitmapDrawable) button.getDrawable()).getBitmap()));
            Assert.assertTrue("Bottom Bar button is not visible.",
                    button.getVisibility() == View.VISIBLE && button.getHeight() > 0
                            && button.getWidth() > 0);
            Assert.assertEquals("Bottom Bar button does not have correct content description",
                    Integer.toString(i + 1), button.getContentDescription());
        }
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testSetTopBarContentView() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ThreadUtils.runOnUiThread(() -> {
            CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
            View anyView = new View(cctActivity);
            cctActivity.setTopBarContentView(anyView);
            ViewGroup topBar = cctActivity.findViewById(R.id.topbar);
            Assert.assertNotNull(topBar);
            Assert.assertThat(anyView.getParent(), equalTo(topBar));
        });
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testSetTopBarContentView_secondCallIsNoOp() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ThreadUtils.runOnUiThread(() -> {
            CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
            View anyView = new View(cctActivity);
            cctActivity.setTopBarContentView(anyView);
            // Second call will not crash.
            cctActivity.setTopBarContentView(anyView);
        });
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testRemoteViews() throws Exception {
        Intent intent = createMinimalCustomTabIntent();

        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        PendingIntent pi = addActionButtonToIntent(intent, expectedIcon, "Good test");

        // Create a RemoteViews. The layout used here is pretty much arbitrary, but with the
        // constraint that a) it already exists in production code, and b) it only contains views
        // with the @RemoteView annotation.
        RemoteViews remoteViews =
                new RemoteViews(InstrumentationRegistry.getTargetContext().getPackageName(),
                        R.layout.web_notification);
        remoteViews.setTextViewText(R.id.title, "Kittens!");
        remoteViews.setTextViewText(R.id.body, "So fluffy");
        remoteViews.setImageViewResource(R.id.icon, R.drawable.ic_music_note_36dp);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS, remoteViews);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS, new int[] {R.id.icon});
        PendingIntent pi2 = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT, pi2);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        mScreenShooter.shoot("Remote Views");
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLaunchWithSession() throws Exception {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoadNewUrlWithSession() throws Exception {
        final Context context = InstrumentationRegistry.getTargetContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        Assert.assertFalse("CustomTabContentHandler handled intent with wrong session",
                ThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return BrowserSessionContentUtils.handleInActiveContentIfNeeded(
                            CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
                }));
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(mTestPage, () -> getActivity().getActivityTab().getUrl()));
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    intent.setData(Uri.parse(mTestPage2));
                    return BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent);
                }));
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        pageLoadFinishedHelper.waitForCallback(0);
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(mTestPage2, () -> getActivity().getActivityTab().getUrl()));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testCreateNewTab() throws Exception {
        final String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/customtabs/test_window_open.html");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        final TabModelSelector tabSelector =
                mCustomTabActivityTestRule.getActivity().getTabModelSelector();

        final CallbackHelper openTabHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            tabSelector.getModel(false).addObserver(new EmptyTabModelObserver() {
                @Override
                public void didAddTab(Tab tab, @TabLaunchType int type) {
                    openTabHelper.notifyCalled();
                }
            });
        });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "new_window");

        openTabHelper.waitForCallback(0, 1);
        Assert.assertEquals(
                "A new tab should have been created.", 2, tabSelector.getModel(false).getCount());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testReferrerAddedAutomatically() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        Assert.assertEquals(referrer, connection.getReferrerForSession(session).getUrl());

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                Assert.assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(
                        () -> BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testVerifiedReferrer() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        String referrer = "https://example.com";
        intent.putExtra(Intent.EXTRA_REFERRER_NAME, referrer);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.addVerifiedOriginForPackage("app1", new Origin(referrer),
                        CustomTabsService.RELATION_USE_AS_ORIGIN));

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                Assert.assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(
                        () -> BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    /**
     * Tests that the navigation callbacks are sent.
     */
    @Test
    @SmallTest
    public void testCallbacksAreSent() throws Exception {
        final Semaphore navigationStartSemaphore = new Semaphore(0);
        final Semaphore navigationFinishedSemaphore = new Semaphore(0);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_FAILED, navigationEvent);
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_ABORTED, navigationEvent);
                if (navigationEvent == CustomTabsCallback.NAVIGATION_STARTED) {
                    navigationStartSemaphore.release();
                } else if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) {
                    navigationFinishedSemaphore.release();
                }
            }
        });
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(
                navigationStartSemaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
        Assert.assertTrue(navigationFinishedSemaphore.tryAcquire(
                TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
    }

    /**
     * Tests that page load metrice are sent.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPageLoadMetricsAreSent() throws Exception {
        checkPageLoadMetrics(true);
    }

    /**
     * Tests that page load metrics are not sent when the client is not whitelisted.
     */
    @Test
    @SmallTest
    public void testPageLoadMetricsAreNotSentByDefault() throws Exception {
        checkPageLoadMetrics(false);
    }

    private static void assertSuffixedHistogramTotalCount(long expected, String histogramPrefix) {
        for (String suffix : new String[] {".ZoomedIn", ".ZoomedOut"}) {
            Assert.assertEquals(expected,
                    RecordHistogram.getHistogramTotalCountForTesting(histogramPrefix + suffix));
        }
    }

    /**
     * Tests that one navigation in a custom tab records the histograms reflecting time from
     * intent to first navigation start/commit.
     */
    @Test
    @SmallTest
    public void testNavigationHistogramsRecorded() throws Exception {
        String startHistogramPrefix = "CustomTabs.IntentToFirstNavigationStartTime";
        String commitHistogramPrefix = "CustomTabs.IntentToFirstCommitNavigationTime3";
        assertSuffixedHistogramTotalCount(0, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(0, commitHistogramPrefix);

        final Semaphore semaphore = new Semaphore(0);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) semaphore.release();
            }
        });
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));

        assertSuffixedHistogramTotalCount(1, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(1, commitHistogramPrefix);
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set onload.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitle() throws Exception {
        final String url = mWebServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set during prerendering.

     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitleHiddenTab() throws Exception {
        final String url = mWebServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, true, "nytimes.com");
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set delayed after load.

     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithDelayedTitle() throws Exception {
        final String url = mWebServer.setResponse("/test.html", DELAYED_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
    }

    private void hideDomainAndEnsureTitleIsSet(
            final String url, boolean useHiddenTab, final String expectedTitle) throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setHideDomainForSession(token, true);

        if (useHiddenTab) {
            connection.setCanUseHiddenTabForSession(token, true);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            ensureCompletedSpeculationForUrl(connection, url);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CustomTabToolbar toolbar =
                        mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
                TextView titleBar = toolbar.findViewById(R.id.title_bar);
                return titleBar != null && titleBar.isShown()
                        && (titleBar.getText()).toString().equals(expectedTitle);
            }
        });
    }

    /**
     * Tests that basic postMessage functionality works through sending a single postMessage
     * request.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageBasic() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> mCustomTabActivityTestRule.getActivity().getActivityTab().loadUrl(
                                new LoadUrlParams(mTestPage2)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return currentTab.isLoadingAndRenderingDone();
            }
        });
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests that postMessage channel is not functioning after web contents get destroyed and also
     * not breaking things.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageWebContentsDestroyed() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);

        final CallbackHelper renderProcessCallback = new CallbackHelper();
        new WebContentsObserver(mCustomTabActivityTestRule.getWebContents()) {
            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                renderProcessCallback.notifyCalled();
            }
        };
        ThreadUtils.postOnUiThread(() -> {
            WebContentsUtils.simulateRendererKilled(
                    mCustomTabActivityTestRule.getActivity().getActivityTab().getWebContents(),
                    false);
        });
        renderProcessCallback.waitForCallback(0);
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests whether validatePostMessageOrigin is necessary for making successful postMessage
     * requests.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageRequiresValidation() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests the sent postMessage requests not only return success, but is also received by page.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedInPage() throws Exception {
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(connection.postMessage(token, "New title", null)
                == CustomTabsService.RESULT_SUCCESS);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return "New title".equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPage() throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
                    @Override
                    public void onMessageChannelReady(Bundle extras) {
                        messageChannelHelper.notifyCalled();
                    }

                    @Override
                    public void onPostMessage(String message, Bundle extras) {
                        onPostMessageHelper.notifyCalled();
                    }
                });
        session.requestPostMessageChannel(null);
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Assert.assertTrue(session.postMessage("Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        messageChannelHelper.waitForCallback(0);
        onPostMessageHelper.waitForCallback(0);
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side even though
     * the request is sent after the page is created.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPageWithLateRequest() throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
                    @Override
                    public void onMessageChannelReady(Bundle extras) {
                        messageChannelHelper.notifyCalled();
                    }

                    @Override
                    public void onPostMessage(String message, Bundle extras) {
                        onPostMessageHelper.notifyCalled();
                    }
                });

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        session.requestPostMessageChannel(null);

        messageChannelHelper.waitForCallback(0);
        onPostMessageHelper.waitForCallback(0);

        Assert.assertTrue(session.postMessage("Message", null) == CustomTabsService.RESULT_SUCCESS);
    }

    private static final int BEFORE_MAY_LAUNCH_URL = 0;
    private static final int BEFORE_INTENT = 1;
    private static final int AFTER_INTENT = 2;

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent before the hidden tab start.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestBeforeMayLaunchUrl() throws Exception {
        sendPostMessageDuringHiddenTabTransition(BEFORE_MAY_LAUNCH_URL);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after the hidden tab start and before intent launched.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestBeforeIntent() throws Exception {
        sendPostMessageDuringHiddenTabTransition(BEFORE_INTENT);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after intent received.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestAfterIntent() throws Exception {
        sendPostMessageDuringHiddenTabTransition(AFTER_INTENT);
    }

    private void sendPostMessageDuringHiddenTabTransition(int requestTime) throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();

        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
                    @Override
                    public void onMessageChannelReady(Bundle extras) {
                        messageChannelHelper.notifyCalled();
                    }
                });

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        boolean channelRequested = false;
        String titleString = "";

        if (requestTime == BEFORE_MAY_LAUNCH_URL) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
        }

        connection.setCanUseHiddenTabForSession(token, true);
        session.mayLaunchUrl(Uri.parse(url), null, null);
        ensureCompletedSpeculationForUrl(connection, url);

        if (requestTime == BEFORE_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
        }

        if (channelRequested) {
            messageChannelHelper.waitForCallback(0);
            String currentMessage = "Prerendering ";
            // Initial title update during prerender.
            Assert.assertTrue(
                    session.postMessage(currentMessage, null) == CustomTabsService.RESULT_SUCCESS);
            titleString = currentMessage;
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        if (requestTime == AFTER_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
            messageChannelHelper.waitForCallback(0);
        }

        String currentMessage = "and loading ";
        // Update title again and verify both updates went through with the channel still intact.
        Assert.assertTrue(
                session.postMessage(currentMessage, null) == CustomTabsService.RESULT_SUCCESS);
        titleString += currentMessage;

        // Request a new channel, verify it was created.
        session.requestPostMessageChannel(null);
        messageChannelHelper.waitForCallback(1);

        String newMessage = "and refreshing";
        // Update title again and verify both updates went through with the channel still intact.
        Assert.assertTrue(
                session.postMessage(newMessage, null) == CustomTabsService.RESULT_SUCCESS);
        titleString += newMessage;

        final String title = titleString;
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return title.equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests that when we use a pre-created renderer, the page loaded is the
     * only one in the navigation history.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPrecreatedRenderer() throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        // Forcing no hidden tab implies falling back to simply creating a spare WebContents.
        connection.setCanUseHiddenTabForSession(token, false);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });

        Assert.assertFalse(mCustomTabActivityTestRule.getActivity().getActivityTab().canGoBack());
        Assert.assertFalse(
                mCustomTabActivityTestRule.getActivity().getActivityTab().canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(1, history.size());
        Assert.assertEquals(mTestPage, history.get(0).getUrl());
    }

    /** Tests that calling warmup() is optional without prerendering. */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupNoSpeculation() throws InterruptedException {
        mayLaunchUrlWithoutWarmup(false);
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupHiddenTab() throws InterruptedException {
        mayLaunchUrlWithoutWarmup(true);
    }

    /**
     * Tests that launching a regular Chrome tab after warmup() gives the right layout.
     *
     * About the restrictions and switches: No FRE and no document mode to get a
     * ChromeTabbedActivity, and no tablets to have the tab switcher button.
     *
     * Non-regression test for crbug.com/547121.
     * @SmallTest
     * Disabled for flake: https://crbug.com/692025.
     */
    @Test
    @DisabledTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRegularChrome() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        Intent intent = new Intent(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        ChromeTabbedActivity.class.getName(), null, false);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity) monitor.waitForActivityWithTimeout(
                        ChromeActivityTestRule.getActivityStartTimeoutMs());
        Assert.assertNotNull("ChromeTabbedActivity did not start", tabbedActivity);
        Assert.assertNotNull("Should have a tab switcher button.",
                tabbedActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests that launching a Custom Tab after warmup() gives the right layout.
     *
     * Non-regression test for crbug.com/547121.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRightToolbarLayout() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mCustomTabActivityTestRule.startActivityCompletely(createMinimalCustomTabIntent());
        Assert.assertNull("Should not have a tab switcher button.",
                getActivity().findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * The expected behavior is that the hidden tab shouldn't be dropped, and that the fragment is
     * updated.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabAndChangingFragmentIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(true, true);
    }

    /** Same as above, but the hidden tab matching should not ignore the fragment. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabAndChangingFragmentDontIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(false, true);
    }

    /** Same as above, hidden tab matching ignores the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabAndChangingFragmentDontWait() throws Exception {
        startHiddenTabAndChangeFragment(true, false);
    }

    /** Same as above, hidden tab matching doesn't ignore the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabAndChangingFragmentDontWaitDrop() throws Exception {
        startHiddenTabAndChangeFragment(false, false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_PARALLEL_REQUEST)
    public void testParallelRequest() throws Exception {
        String url = mTestServer.getURL("/echoheader?Cookie");
        Uri requestUri = Uri.parse(mTestServer.getURL("/set-cookie?acookie"));

        final Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        // warmup(), create session, allow parallel requests, allow origin.
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final Origin origin = new Origin(requestUri);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setAllowParallelRequestForSession(token, true);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerifiedOriginForPackage(
                    context.getPackageName(), origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
        });

        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, requestUri);
        intent.putExtra(
                CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, Uri.parse(origin.toString()));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * There are two parameters changing the bahavior:
     * @param ignoreFragments Whether the hidden tab should be kept.
     * @param wait Whether to wait for the hidden tab to load.
     */
    private void startHiddenTabAndChangeFragment(boolean ignoreFragments, boolean wait)
            throws Exception {
        String testUrl = mTestServer.getURL(FRAGMENT_TEST_PAGE);
        String initialFragment = "#test";
        String initialUrl = testUrl + initialFragment;
        String fragment = "#yeah";
        String urlWithFragment = testUrl + fragment;

        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, urlWithFragment);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setIgnoreUrlFragmentsForSession(token, ignoreFragments);
        connection.setCanUseHiddenTabForSession(token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(initialUrl), null, null));

        if (wait) {
            ensureCompletedSpeculationForUrl(connection, initialUrl);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = getActivity().getActivityTab();

        if (wait) {
            ElementContentCriteria initialVisibilityCriteria = new ElementContentCriteria(
                    tab, "visibility", ignoreFragments ? "hidden" : "visible");
            ElementContentCriteria initialFragmentCriteria = new ElementContentCriteria(
                    tab, "initial-fragment", ignoreFragments ? initialFragment : fragment);
            ElementContentCriteria fragmentCriteria =
                    new ElementContentCriteria(tab, "fragment", fragment);
            // The tab hasn't been reloaded.
            CriteriaHelper.pollInstrumentationThread(initialVisibilityCriteria, 2000, 200);
            // No reload (initial fragment is correct).
            CriteriaHelper.pollInstrumentationThread(initialFragmentCriteria, 2000, 200);
            if (ignoreFragments) {
                CriteriaHelper.pollInstrumentationThread(fragmentCriteria, 2000, 200);
            }
        } else {
            CriteriaHelper.pollInstrumentationThread(new ElementContentCriteria(
                    tab, "initial-fragment", fragment), 2000, 200);
        }

        Assert.assertFalse(tab.canGoForward());
        Assert.assertFalse(tab.canGoBack());

        // TODO(ahemery):
        // Fragment misses will trigger two history entries
        // - url#speculated and url#actual are both inserted
        // This should ideally not be the case.
    }

    /**
     * Test whether the url shown on hidden tab gets updated from about:blank when it
     * completes in the background.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabCorrectUrl() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        ensureCompletedSpeculationForUrl(connection, mTestPage);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        Assert.assertEquals(Uri.parse(mTestPage).getHost() + ":" + Uri.parse(mTestPage).getPort(),
                ((EditText) getActivity().findViewById(R.id.url_bar)).getText().toString());
    }

    /**
     * Test that hidden tab speculation is not performed if 3rd party cookies are blocked.
     **/
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    public void testHiddenTabThirdPartyCookiesBlocked() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, true);
        connection.warmup(0);

        // Needs the browser process to be initialized.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            boolean old_block_pref = prefs.isBlockThirdPartyCookiesEnabled();
            prefs.setBlockThirdPartyCookiesEnabled(false);
            Assert.assertTrue(connection.maySpeculate(token));
            prefs.setBlockThirdPartyCookiesEnabled(true);
            Assert.assertFalse(connection.maySpeculate(token));
            prefs.setBlockThirdPartyCookiesEnabled(old_block_pref);
        });
    }

    /**
     * Test whether invalid urls are avoided for hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabInvalidUrl() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, true);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse("chrome://version"), null, null));
    }

    /**
     * Tests that the activity knows there is already a child process when warmup() has been called.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithWarmup() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(
                        "Warmup() should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection()));
    }

    /**
     * Tests that the activity knows there is no child process.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionNoWarmup() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsConnection.getInstance();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertTrue(
                        "No spare renderer available, should allocate a child connection.",
                        getActivity().shouldAllocateChildConnection()));
    }

    /**
     * Tests that the activity knows there is already a child process with a hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithHiddenTab() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(
                        "Prerendering should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection()));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecreateSpareRendererOnTabClose() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsTestUtils.warmUpAndWait();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
            activity.finishAndClose(false);
        });
        CriteriaHelper.pollUiThread(new Criteria("No new spare renderer") {
            @Override
            public boolean isSatisfied() {
                return WarmupManager.getInstance().hasSpareWebContents();
            }
        }, 2000, 200);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is not lost when launching the
     * Custom Tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabWithReferrer() throws Exception {
        String referrer = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, referrer, referrer);

        Tab tab = getActivity().getActivityTab();
        // The tab hasn't been reloaded.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "hidden"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrer), 2000, 200);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is dropped when the tab
     * is launched with a mismatched referrer.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabWithMismatchedReferrers() throws Exception {
        String prerenderReferrer = "android-app://com.foo.me/";
        String launchReferrer = "android-app://com.foo.me.i.changed.my.mind/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, prerenderReferrer, launchReferrer);

        Tab tab = getActivity().getActivityTab();
        // Prerender has been dropped.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "visible"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, launchReferrer), 2000, 200);
    }

    /** Tests that a client can set a referrer, without speculating. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testClientCanSetReferrer() throws Exception {
        String referrerUrl = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(mTestPage, false, null, referrerUrl);

        Tab tab = getActivity().getActivityTab();
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrerUrl), 2000, 200);
    }

    @Test
    @MediumTest
    public void testLaunchIncognitoURL() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
        final CallbackHelper mCctHiddenCallback = new CallbackHelper();
        final CallbackHelper mTabbedModeShownCallback = new CallbackHelper();
        final AtomicReference<ChromeTabbedActivity> tabbedActivity = new AtomicReference<>();

        ActivityStateListener listener = (activity, newState) -> {
            if (activity == cctActivity
                    && (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED)) {
                mCctHiddenCallback.notifyCalled();
            }

            if (activity instanceof ChromeTabbedActivity && newState == ActivityState.RESUMED) {
                mTabbedModeShownCallback.notifyCalled();
                tabbedActivity.set((ChromeTabbedActivity) activity);
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(listener);

        ThreadUtils.runOnUiThread(() -> {
            cctActivity.getActivityTab().getTabWebContentsDelegateAndroid().openNewTab(
                    "about:blank", null, null, WindowOpenDisposition.OFF_THE_RECORD, false);
        });

        mCctHiddenCallback.waitForCallback("CCT not hidden.", 0);
        mTabbedModeShownCallback.waitForCallback("Tabbed mode not shown.", 0);

        CriteriaHelper.pollUiThread(
                Criteria.equals(true, () -> tabbedActivity.get().areTabModelsInitialized()));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = tabbedActivity.get().getActivityTab();
                if (tab == null) {
                    updateFailureReason("Tab is null");
                    return false;
                }
                if (!tab.isIncognito()) {
                    updateFailureReason("Incognito tab not selected");
                    return false;
                }
                if (!TextUtils.equals(tab.getUrl(), "about:blank")) {
                    updateFailureReason("Wrong URL loaded in incognito tab");
                    return false;
                }
                return true;
            }
        });

        ApplicationStatus.unregisterActivityStateListener(listener);
    }

    @Test
    @MediumTest
    public void testLaunchIncognitoCustomTabForPaymentRequest() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        CustomTabIntentDataProvider.addPaymentRequestUIExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Assert.assertTrue(mCustomTabActivityTestRule.getActivity().getActivityTab().isIncognito());
    }

    /**
     * Tests that a Weblite URL from an external app uses the lite_url param when Data Reduction
     * Proxy previews are being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-spdy-proxy-auth", "enable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mTestPage, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy previews are not being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-spdy-proxy-auth", "disable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoPreviews() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy is not being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoDataReductionProxy() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when the param
     * is an https URL.
     */
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-spdy-proxy-auth", "enable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchHttpsWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage.replaceFirst("http", "https");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a URL from an external app does not use the lite_url param when the prefix is not
     * the WebLite url.
     */
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-spdy-proxy-auth", "enable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchNonWebLiteURL() throws Exception {
        final String testUrl = mTestPage2 + "/?lite_url=" + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /** Maybe prerenders a URL with a referrer, then launch it with another one. */
    private void maybeSpeculateAndLaunchWithReferrers(String url, boolean useHiddenTab,
            String speculationReferrer, String launchReferrer) throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        if (useHiddenTab) {
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            connection.newSession(token);
            connection.setCanUseHiddenTabForSession(token, true);
            Bundle extras = null;
            if (speculationReferrer != null) {
                extras = new Bundle();
                extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(speculationReferrer));
            }
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), extras, null));
            ensureCompletedSpeculationForUrl(connection, url);
        }

        if (launchReferrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(launchReferrer));
        }
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * Test intended to verify the way we test history is correct.
     * Start an activity and then navigate to a different url.
     * We test NavigationController behavior through canGoBack/Forward as well
     * as browser history through an HistoryProvider.
     */
    @Test
    @SmallTest
    public void testHistoryNoSpeculation() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        Assert.assertTrue(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(2, history.size());
        Assert.assertEquals(mTestPage2, history.get(0).getUrl());
        Assert.assertEquals(mTestPage, history.get(1).getUrl());
    }

    /**
     * The following test that history only has a single final page after speculation,
     * whether it was a hit or a miss.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabHit() throws Exception {
        verifyHistoryAfterHiddenTab(true);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabMiss() throws Exception {
        verifyHistoryAfterHiddenTab(false);
    }

    private void verifyHistoryAfterHiddenTab(boolean speculationWasAHit) throws Exception {
        String speculationUrl = mTestPage;
        String navigationUrl = speculationWasAHit ? mTestPage : mTestPage2;
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, navigationUrl);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, true);

        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(speculationUrl), null, null));
        ensureCompletedSpeculationForUrl(connection, speculationUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = getActivity().getActivityTab();
        Assert.assertFalse(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(1, history.size());
        Assert.assertEquals(navigationUrl, history.get(0).getUrl());
    }

    private void mayLaunchUrlWithoutWarmup(boolean useHiddenTab) throws InterruptedException {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        Bundle extras = null;
        connection.setCanUseHiddenTabForSession(token, useHiddenTab);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mTestPage, tab.getUrl());
    }

    private ChromeActivity reparentAndVerifyTab() throws InterruptedException {
        final ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                ChromeTabbedActivity.class.getName(), /* result = */ null, false);
        final Tab tabToBeReparented = getActivity().getActivityTab();
        final CallbackHelper tabHiddenHelper = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                tabHiddenHelper.notifyCalled();
            }
        };
        tabToBeReparented.addObserver(observer);
        ThreadUtils.postOnUiThread(() -> {
            getActivity().openCurrentUrlInBrowser(true);
            Assert.assertNull(getActivity().getActivityTab());
        });
        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity lastActivity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(
                "Monitor did not get an activity before hitting the timeout", lastActivity);
        Assert.assertTrue("Expected lastActivity to be a ChromeActivity, was "
                        + lastActivity.getClass().getName(),
                lastActivity instanceof ChromeActivity);
        final ChromeActivity newActivity = (ChromeActivity) lastActivity;
        CriteriaHelper.pollUiThread((new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newActivity.getActivityTab() != null
                        && newActivity.getActivityTab().equals(tabToBeReparented);
            }
        }));
        Assert.assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        Assert.assertEquals(newActivity.getWindowAndroid(),
                tabToBeReparented.getWebContents().getTopLevelNativeWindow());
        Assert.assertFalse(
                tabToBeReparented.getDelegateFactory() instanceof CustomTabDelegateFactory);
        Assert.assertEquals("The tab should never be hidden during the reparenting process", 0,
                tabHiddenHelper.getCallCount());
        Assert.assertFalse(tabToBeReparented.isCurrentlyACustomTab());
        tabToBeReparented.removeObserver(observer);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(tabToBeReparented);
        while (observers.hasNext()) {
            Assert.assertFalse(observers.next() instanceof CustomTabObserver);
        }
        return newActivity;
    }

    private void checkPageLoadMetrics(boolean allowMetrics)
            throws InterruptedException, TimeoutException {
        final AtomicReference<Long> firstContentfulPaintMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> activityStartTimeMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> loadEventStartMs = new AtomicReference<>(-1L);
        final AtomicReference<Boolean> sawNetworkQualityEstimates = new AtomicReference<>(false);

        CustomTabsCallback cb = new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) return;

                Assert.assertEquals(CustomTabsConnection.PAGE_LOAD_METRICS_CALLBACK, callbackName);
                if (-1 != args.getLong(PageLoadMetrics.EFFECTIVE_CONNECTION_TYPE, -1)) {
                    sawNetworkQualityEstimates.set(true);
                }

                long navigationStart = args.getLong(PageLoadMetrics.NAVIGATION_START, -1);
                if (navigationStart == -1) {
                    // Untested metric callback.
                    return;
                }
                long current = SystemClock.uptimeMillis();
                Assert.assertTrue(navigationStart <= current);
                Assert.assertTrue(navigationStart >= activityStartTimeMs.get());

                long firstContentfulPaint =
                        args.getLong(PageLoadMetrics.FIRST_CONTENTFUL_PAINT, -1);
                if (firstContentfulPaint > 0) {
                    Assert.assertTrue(firstContentfulPaint <= (current - navigationStart));
                    firstContentfulPaintMs.set(firstContentfulPaint);
                }

                long loadEventStart = args.getLong(PageLoadMetrics.LOAD_EVENT_START, -1);
                if (loadEventStart > 0) {
                    Assert.assertTrue(loadEventStart <= (current - navigationStart));
                    loadEventStartMs.set(loadEventStart);
                }
            }
        };

        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(cb);
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;

        if (allowMetrics) {
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            connection.mClientManager.setShouldGetPageLoadMetricsForSession(token, true);
        }

        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        activityStartTimeMs.set(SystemClock.uptimeMillis());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        if (allowMetrics) {
            CriteriaHelper.pollInstrumentationThread(() -> firstContentfulPaintMs.get() > 0);
            CriteriaHelper.pollInstrumentationThread(() -> loadEventStartMs.get() > 0);
            CriteriaHelper.pollInstrumentationThread(() -> sawNetworkQualityEstimates.get());
        } else {
            try {
                CriteriaHelper.pollInstrumentationThread(() -> firstContentfulPaintMs.get() > 0);
            } catch (AssertionError e) {
                // Expected.
            }
            Assert.assertEquals(-1L, (long) firstContentfulPaintMs.get());
        }
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession(Intent intentWithSession)
            throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intentWithSession);
        connection.newSession(token);
        intentWithSession.setData(Uri.parse(mTestPage));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intentWithSession);
        return token;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession() throws Exception {
        return warmUpAndLaunchUrlWithSession(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage));
    }

    private static void ensureCompletedSpeculationForUrl(
            final CustomTabsConnection connection, final String url) throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria("Tab was not created") {
            @Override
            public boolean isSatisfied() {
                return connection.mSpeculation != null && connection.mSpeculation.tab != null;
            }
        }, LONG_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeTabUtils.waitForTabPageLoaded(connection.mSpeculation.tab, url);
    }

    /**
      * A helper class to monitor sending status of a {@link PendingIntent}.
      */
    private class OnFinishedForTest implements PendingIntent.OnFinished {

        private final PendingIntent mPendingIntent;
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private Intent mCallbackIntent;

        /**
         * Create an instance of {@link OnFinishedForTest}, testing the given {@link PendingIntent}.
         */
        public OnFinishedForTest(PendingIntent pendingIntent) {
            mPendingIntent = pendingIntent;
        }

        public Intent getCallbackIntent() {
            return mCallbackIntent;
        }

        public void waitForCallback(String failureReason)
                throws TimeoutException, InterruptedException {
            mCallbackHelper.waitForCallback(failureReason, 0);
        }

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            if (pendingIntent.equals(mPendingIntent)) {
                mCallbackIntent = intent;
                mCallbackHelper.notifyCalled();
            }
        }
    }

    private static class ElementContentCriteria extends Criteria {
        private final Tab mTab;
        private final String mJsFunction;
        private final String mExpected;

        public ElementContentCriteria(Tab tab, String elementId, String expected) {
            super("Page element is not as expected.");
            mTab = tab;
            mExpected = "\"" + expected + "\"";
            mJsFunction = "(function () { return document.getElementById(\"" + elementId
                    + "\").innerHTML; })()";
        }

        @Override
        public boolean isSatisfied() {
            String value;
            try {
                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), mJsFunction);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                value = jsonText;
            } catch (InterruptedException | TimeoutException e) {
                e.printStackTrace();
                return false;
            }
            boolean isSatisfied = TextUtils.equals(mExpected, value);
            if (!isSatisfied) {
              updateFailureReason("Page element is " + value + " instead of expected " + mExpected);
            }
            return isSatisfied;
        }
    }

    private static List<HistoryItem> getHistory() throws TimeoutException, InterruptedException {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(false);
            historyService.setObserver(historyObserver);
            String historyQueryFilter = "";
            historyService.queryHistory(historyQueryFilter);
        });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }
}
