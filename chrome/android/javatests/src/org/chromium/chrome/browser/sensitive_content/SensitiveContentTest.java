// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.MockLayoutHost;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.SwipingToTabFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests that the content sensitivity of is set properly. The test fixture uses a tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/377495440): Try to batch the tests.
@DoNotBatch(
        reason =
                "Test have complex logic, and individual set-ups of some tests get in the way of"
                        + " other tests")
@EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT)
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SensitiveContentTest {
    private static final class TestSensitiveContentClientObserver
            implements SensitiveContentClient.Observer {
        private boolean mContentIsSensitive;

        @Override
        public void onContentSensitivityChanged(boolean contentIsSensitive) {
            mContentIsSensitive = contentIsSensitive;
        }

        public boolean getContentSensitivity() {
            return mContentIsSensitive;
        }
    }

    public static final String SENSITIVE_FILE =
            "/chrome/test/data/autofill/autofill_creditcard_form_with_autocomplete_attributes.html";
    public static final String NOT_SENSITIVE_FILE =
            "/chrome/test/data/autofill/autocomplete_simple_form.html";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private HubLayoutDependencyHolder mHubLayoutDependencyHolder;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private ObservableSupplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    @Mock private ObservableSupplier<Boolean> mScrimVisibilitySupplier;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private ViewGroup mContentView;

    private WebPageStation mPage;
    private EmbeddedTestServer mTestServer;
    private TabModelSelector mTabModelSelector;
    private LayoutManagerChromePhone mLayoutManagerChromePhone;

    @Before
    public void setUp() throws Exception {
        mPage = mCtaTestRule.startOnBlankPage();
        mTestServer = mCtaTestRule.getTestServer();
    }

    @Test
    @MediumTest
    public void testTabHasSensitiveContentWhileSensitiveFieldsArePresent() {
        assertNotSensitive(mPage);

        PageStation page = mPage.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        assertSensitive(page);

        page = page.loadWebPageProgrammatically(mTestServer.getURL(NOT_SENSITIVE_FILE));
        assertNotSensitive(page);
    }

    @Test
    @MediumTest
    public void testSensitiveContentClientObserver() {
        assertNotSensitive(mPage);

        final SensitiveContentClient client =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SensitiveContentClient.fromWebContents(
                                        mCtaTestRule.getActivityTestRule().getWebContents()));
        final TestSensitiveContentClientObserver observer =
                new TestSensitiveContentClientObserver();
        ThreadUtils.runOnUiThreadBlocking(() -> client.addObserver(observer));

        assertFalse(observer.getContentSensitivity());
        PageStation page = mPage.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        assertSensitive(page);
        assertTrue(observer.getContentSensitivity());

        page = page.loadWebPageProgrammatically(mTestServer.getURL(NOT_SENSITIVE_FILE));
        assertNotSensitive(page);
        assertFalse(observer.getContentSensitivity());

        // After observation is removed, the observer will not be notified anymore.
        ThreadUtils.runOnUiThreadBlocking(() -> client.removeObserver(observer));
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        assertSensitive(page);
        assertFalse(observer.getContentSensitivity());
    }

    @Test
    @MediumTest
    public void testSwapViewAndroidDelegate() {
        mPage.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContents webContents = mCtaTestRule.getActivityTestRule().getWebContents();
                    ContentView newContainerView =
                            ContentView.createContentView(mCtaTestRule.getActivity(), webContents);
                    ViewAndroidDelegate newViewAndroidDelegate =
                            ViewAndroidDelegate.createBasicDelegate(newContainerView);
                    assertEquals(
                            "Initially, the content view does not have sensitive content",
                            View.CONTENT_SENSITIVITY_AUTO,
                            newContainerView.getContentSensitivity());

                    webContents.setDelegates(
                            "",
                            newViewAndroidDelegate,
                            newContainerView,
                            null,
                            WebContents.createDefaultInternalsHolder());
                    pollUiThreadNested(
                            () ->
                                    newContainerView.getContentSensitivity()
                                            == View.CONTENT_SENSITIVITY_SENSITIVE);
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testTabHasSensitiveContentAttributeIsUpdated() {
        final Tab tab = mCtaTestRule.getActivity().getActivityTab();
        assertFalse(tab.getTabHasSensitiveContent());

        PageStation page = mPage.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        assertSensitive(page);
        assertTrue(tab.getTabHasSensitiveContent());

        page = page.loadWebPageProgrammatically(mTestServer.getURL(NOT_SENSITIVE_FILE));
        assertNotSensitive(page);
        assertFalse(tab.getTabHasSensitiveContent());
    }

    // This test also tests if metrics are recorded properly.
    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testRegularTabSwitcherBecomesSensitive() {
        final String histogram = "SensitiveContent.TabSwitching.RegularTabSwitcherPane.Sensitivity";
        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        // Open a second tab.
        PageStation page = mPage.openNewTabFast();
        final Tab secondTab = page.loadedTabElement.get();
        // Load sensitive content only into the second tab.
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondTab.getTabHasSensitiveContent());
        // Open the tab switcher.
        RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is sensitive.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ true);

        // Close the second tab (the only tab with sensitive content).
        regularTabSwitcher = regularTabSwitcher.closeTabAtIndex(1, RegularTabSwitcherStation.class);
        // Select the only remaining tab.
        page = regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        // Open the tab switcher.
        regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is not sensitive anymore.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ false);
        histogramWatcherForFalseBucket.assertExpected();

        // Go back to a tab to cleanup tab state.
        regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
    }

    // This test also tests if metrics are recorded properly.
    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testIncognitoTabSwitcherBecomesSensitive() {
        final String histogram =
                "SensitiveContent.TabSwitching.IncognitoTabSwitcherPane.Sensitivity";
        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        // Open the first incognito tab.
        PageStation page = mPage.openNewIncognitoTabFast();
        // Open the second incognito tab.
        page = page.openNewIncognitoTabFast();
        final Tab secondIncognitoTab = page.loadedTabElement.get();
        // Load sensitive content only into the second incognito tab.
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondIncognitoTab.getTabHasSensitiveContent());
        // Open the incognito tab switcher.
        IncognitoTabSwitcherStation incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is sensitive.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        // Close the second incognito tab (the only tab with sensitive content).
        incognitoTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(1, IncognitoTabSwitcherStation.class);
        // Select the only remaining incognito tab.
        page = incognitoTabSwitcher.selectTabAtIndex(0, IncognitoNewTabPageStation.newBuilder());
        // Open the incognito tab switcher.
        incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is not sensitive anymore.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ false);
        histogramWatcherForFalseBucket.assertExpected();

        // Go back to a tab to cleanup tab state.
        incognitoTabSwitcher.selectTabAtIndex(0, IncognitoNewTabPageStation.newBuilder());
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testRegularTabSwitcherBecomesSensitiveWithTabGroups() {
        final Tab firstTab = mPage.loadedTabElement.get();
        // Open a second tab.
        PageStation page = mPage.openNewTabFast();
        final Tab secondTab = page.loadedTabElement.get();
        // Load sensitive content only into the second tab.
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondTab.getTabHasSensitiveContent());
        // Group the tabs.
        TabUiTestHelper.createTabGroup(
                mCtaTestRule.getActivity(), false, List.of(firstTab, secondTab));
        // Open the tab switcher.
        final RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is sensitive.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ true);

        // Go back to a tab to cleanup tab state. It is easier to open a new tab than to go to an
        // existing tab.
        regularTabSwitcher.openAppMenu().openNewTab();
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testIncognitoTabSwitcherBecomesSensitiveWithTabGroups() {
        // Open the first incognito tab.
        PageStation page = mPage.openNewIncognitoTabFast();
        final Tab firstIncognitoTab = page.loadedTabElement.get();
        // Open the second incognito tab.
        page = page.openNewIncognitoTabFast();
        final Tab secondIncognitoTab = page.loadedTabElement.get();
        // Load sensitive content only into the second incognito tab.
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondIncognitoTab.getTabHasSensitiveContent());
        // Group the incognito tabs.
        TabUiTestHelper.createTabGroup(
                mCtaTestRule.getActivity(), true, List.of(firstIncognitoTab, secondIncognitoTab));
        // Open the incognito tab switcher.
        final IncognitoTabSwitcherStation incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is sensitive.
        checkContentSensitivityOfViewWithId(
                R.id.tab_list_recycler_view, /* contentIsSensitive= */ true);

        // Go back to a tab to cleanup tab state. It is easier to open a new tab than to go to an
        // existing tab.
        incognitoTabSwitcher.openAppMenu().openNewTab();
    }

    // This test also tests if metrics are recorded properly.
    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    @Restriction(DeviceFormFactor.PHONE)
    public void testTabGroupUiOpenedFromBottomToolbarBecomesSensitive() {
        final String histogram = "SensitiveContent.TabSwitching.BottomTabStripGroupUI.Sensitivity";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        // Load sensitive content only into the first tab.
        final Tab firstTab = mPage.loadedTabElement.get();
        PageStation page = mPage.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> firstTab.getTabHasSensitiveContent());
        // Open a second tab.
        page = page.openNewTabFast();
        final Tab secondTab = page.loadedTabElement.get();
        // Group the tabs.
        TabUiTestHelper.createTabGroup(
                mCtaTestRule.getActivity(), false, List.of(firstTab, secondTab));

        // Click on the "arrow button" from the bottom toolbar to display the tab group UI.
        onView(allOf(withId(R.id.toolbar_show_group_dialog_button))).perform(click());
        // Check that the tab group UI view is sensitive.
        checkContentSensitivityOfViewWithId(
                R.id.dialog_parent_view, /* contentIsSensitive= */ true);
        // Check that the content view is not sensitive. This ensures that the screen won't be
        // redacted if the tab group UI closes.
        assertNotEquals(
                getContentViewOfCurrentTab().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);
        histogramWatcher.assertExpected();
    }

    // This test also tests if metrics are recorded properly.
    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testSwipingBetweenTabsIsSensitive() {
        PageStation page = mPage;
        // Set up.
        // Open a second tab.
        page = page.openNewTabFast();
        // Open a third tab.
        page = page.openNewTabFast();
        // Load sensitive content into the third tab.
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        assertTrue(page.loadedTabElement.get().getTabHasSensitiveContent());
        // Open a fourth tab.
        page = page.openNewTabFast();

        final String histogram = "SensitiveContent.SensitiveTabSwitchingAnimations";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                histogram,
                                SensitiveContentClient.TabSwitchingAnimation.TOP_TOOLBAR_SWIPE,
                                /* times= */ 2)
                        .build();

        SwipingToTabFacility partialSwipe;
        assertNotSensitive(page);

        // Swiping from a not sensitive tab to a sensitive one should mark the content container as
        // sensitive.
        partialSwipe = page.swipeToolbarToPreviousTabPartial();
        assertSensitive(page.getActivity());
        page = partialSwipe.finishSwipe(WebPageStation.newBuilder());
        // After the swipe ends, the content container should return to not being sensitive.
        assertNotSensitive(page.getActivity());
        assertSensitive(page);

        // Swiping from a sensitive tab to a not sensitive one should mark the content container as
        // sensitive.
        partialSwipe = page.swipeToolbarToPreviousTabPartial();
        assertSensitive(page.getActivity());
        page = partialSwipe.finishSwipe(RegularNewTabPageStation.newBuilder());
        // After the swipe ends, the content container should return to not being sensitive.
        assertNotSensitive(page.getActivity());
        assertNotSensitive(page);

        // Swiping between 2 not sensitive tabs should not mark the content container as sensitive.
        partialSwipe = page.swipeToolbarToPreviousTabPartial();
        assertNotSensitive(page.getActivity());
        page = partialSwipe.finishSwipe(WebPageStation.newBuilder());
        // Even after the swipe ends, the content container should not be sensitive.
        assertNotSensitive(page.getActivity());
        assertNotSensitive(page);

        histogramWatcher.assertExpected();
    }

    private void assertNotSensitive(Activity activity) {
        assertNotEquals(
                View.CONTENT_SENSITIVITY_SENSITIVE, getActivityContentSensitivity(activity));
    }

    private void assertSensitive(Activity activity) {
        assertEquals(View.CONTENT_SENSITIVITY_SENSITIVE, getActivityContentSensitivity(activity));
    }

    private int getActivityContentSensitivity(Activity activity) {
        return activity.findViewById(android.R.id.content).getContentSensitivity();
    }

    private void assertNotSensitive(PageStation page) {
        assertNotEquals(View.CONTENT_SENSITIVITY_SENSITIVE, getPageContentSensitivity(page));
    }

    private void assertSensitive(PageStation page) {
        assertEquals(View.CONTENT_SENSITIVITY_SENSITIVE, getPageContentSensitivity(page));
    }

    private int getPageContentSensitivity(PageStation page) {
        return page.loadedTabElement.get().getContentView().getContentSensitivity();
    }

    // The tested animation occurs for example when a link is opened in a new tab or in a new tab in
    // group. This test also tests if metrics are recorded properly.
    @Test
    @MediumTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    @DisableFeatures({ChromeFeatureList.SHOW_NEW_TAB_ANIMATIONS})
    @Restriction(DeviceFormFactor.PHONE)
    public void testSimpleAnimationLayoutHasSensitiveContent() throws TimeoutException {
        final String histogram = "SensitiveContent.SensitiveTabSwitchingAnimations";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogram,
                        SensitiveContentClient.TabSwitchingAnimation.NEW_TAB_IN_BACKGROUND);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    initializeLayoutManagerPhone(
                            /* standardTabCount= */ 1,
                            /* incognitoTabCount= */ 0,
                            TabModel.INVALID_TAB_INDEX,
                            TabModel.INVALID_TAB_INDEX,
                            /* incognitoSelected= */ false);

                    Tab sourceTab =
                            MockTab.createAndInitialize(
                                    /* id= */ 100, ProfileManager.getLastUsedRegularProfile());
                    mTabModelSelector
                            .getModel(/* incognito= */ false)
                            .addTab(
                                    sourceTab,
                                    /* index= */ -1,
                                    TabLaunchType.FROM_CHROME_UI,
                                    TabCreationState.LIVE_IN_FOREGROUND);
                    sourceTab.setTabHasSensitiveContent(true);
                    // Chances are the sensitivity is set to auto initially. That's not a
                    // problem, it just needs not to be sensitive.
                    assertNotEquals(
                            mLayoutManagerChromePhone.getContentContainer().getContentSensitivity(),
                            View.CONTENT_SENSITIVITY_SENSITIVE);

                    Tab newTab =
                            MockTab.createAndInitialize(
                                    /* id= */ 200, ProfileManager.getLastUsedRegularProfile());
                    mTabModelSelector
                            .getModel(/* incognito= */ false)
                            .addTab(
                                    newTab,
                                    /* index= */ -1,
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    TabCreationState.LIVE_IN_BACKGROUND);
                    // The content container should become sensitive before the animation starts.
                    assertEquals(
                            View.CONTENT_SENSITIVITY_SENSITIVE,
                            mLayoutManagerChromePhone
                                    .getContentContainer()
                                    .getContentSensitivity());

                    assertTrue(
                            "LayoutManager took too long to finish the animations",
                            simulateTime(mLayoutManagerChromePhone, 1000));
                    assertEquals(
                            "Incorrect active LayoutType",
                            LayoutType.SIMPLE_ANIMATION,
                            mLayoutManagerChromePhone.getActiveLayout().getLayoutType());
                    assertTrue(
                            "Incorrect active Layout",
                            mLayoutManagerChromePhone.isLayoutVisible(LayoutType.SIMPLE_ANIMATION));
                });

        pollUiThread(
                () -> {
                    return mLayoutManagerChromePhone.getActiveLayout().getLayoutType()
                                    == LayoutType.SIMPLE_ANIMATION
                            && mLayoutManagerChromePhone.getActiveLayout().isStartingToHide();
                });

        // Check that the content container is still sensitive before the animation finishes.
        assertEquals(
                View.CONTENT_SENSITIVITY_SENSITIVE,
                mLayoutManagerChromePhone.getContentContainer().getContentSensitivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Simulate hiding animation.
                    assertTrue(
                            "LayoutManager took too long to finish the animations",
                            simulateTime(mLayoutManagerChromePhone, 1000));
                });
        // Check that the content container is not sensitive anymore after the animation
        // finishes.
        assertEquals(
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE,
                mLayoutManagerChromePhone.getContentContainer().getContentSensitivity());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testSensitiveContentIsRestoredFromTabState() {
        // Create a new tab.
        PageStation page = mPage.openNewTabFast();
        final Tab secondTabBeforeFreeze = page.loadedTabElement.get();
        page = page.loadWebPageProgrammatically(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondTabBeforeFreeze.getTabHasSensitiveContent());

        // Save the state of the second tab (the only tab with sensitive content).
        final TabState state =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> TabStateExtractor.from(secondTabBeforeFreeze));

        Tab[] secondTabAfterFreeze = new Tab[] {null};
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final TabModel tabModel =
                            mCtaTestRule.getActivity().getTabModelSelector().getModel(false);
                    // Close the second tab.
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(secondTabBeforeFreeze)
                                            .allowUndo(false)
                                            .build(),
                                    /* allowDialog= */ false);
                    assertEquals(1, tabModel.getCount());
                    // Re-create the second tab using the same id and tab state.
                    tabModel.getTabCreator()
                            .createFrozenTab(state, secondTabBeforeFreeze.getId(), /* index= */ 1);
                    assertEquals(2, tabModel.getCount());
                    secondTabAfterFreeze[0] = tabModel.getTabAt(1);
                    assertNotNull(secondTabAfterFreeze[0]);
                    assertNull(secondTabAfterFreeze[0].getWebContents());
                });

        // Select the second tab.
        final RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        regularTabSwitcher.selectTabAtIndex(1, WebPageStation.newBuilder());

        // The second tab should have sensitive content. The content sensitivity should have been
        // restored from tab state.
        final SensitiveContentClient client =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SensitiveContentClient.fromWebContents(
                                        secondTabAfterFreeze[0].getWebContents()));
        assertTrue(client.getContentRestoredFromTabStateIsSensitive().orElse(false));
        assertEquals(
                View.CONTENT_SENSITIVITY_SENSITIVE,
                secondTabAfterFreeze[0].getContentView().getContentSensitivity());
    }

    private void checkContentSensitivityOfViewWithId(int viewId, boolean contentIsSensitive) {
        onView(allOf(withId(viewId), isDisplayed()))
                .check(
                        (view, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            assertEquals(
                                    view.getContentSensitivity(),
                                    contentIsSensitive
                                            ? View.CONTENT_SENSITIVITY_SENSITIVE
                                            : View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
                        });
    }

    private View getContentViewOfCurrentTab() {
        return mCtaTestRule.getActivity().getActivityTab().getContentView();
    }

    private void initializeLayoutManagerPhone(
            int standardTabCount,
            int incognitoTabCount,
            int standardIndexSelected,
            int incognitoIndexSelected,
            boolean incognitoSelected) {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mTabModelSelector =
                new MockTabModelSelector(
                        ProfileManager.getLastUsedRegularProfile(),
                        ProfileManager.getLastUsedRegularProfile().getPrimaryOtrProfile(true),
                        standardTabCount,
                        incognitoTabCount,
                        null);
        if (standardIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(false), standardIndexSelected);
        }
        if (incognitoIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(true), incognitoIndexSelected);
        }
        mTabModelSelector.selectModel(incognitoSelected);
        assertNotNull(
                mTabModelSelector.getTabGroupModelFilterProvider().getCurrentTabGroupModelFilter());

        LayoutManagerHost layoutManagerHost = new MockLayoutHost(context);
        TabContentManager tabContentManager =
                new TabContentManager(context, null, false, null, mTabWindowManager);
        tabContentManager.initWithNative();
        ObservableSupplierImpl<TabContentManager> tabContentManagerSupplier =
                new ObservableSupplierImpl<>();
        OneshotSupplierImpl tabSwitcherSupplier = new OneshotSupplierImpl();

        // Build a fake content container
        FrameLayout parentContainer = new FrameLayout(context);
        FrameLayout container = new FrameLayout(context);
        parentContainer.addView(container);

        mLayoutManagerChromePhone =
                new LayoutManagerChromePhone(
                        layoutManagerHost,
                        container,
                        tabSwitcherSupplier,
                        () -> mTabModelSelector,
                        tabContentManagerSupplier,
                        () -> mTopUiThemeColorProvider,
                        mHubLayoutDependencyHolder,
                        mCompositorViewHolderSupplier,
                        mContentView,
                        mToolbarManager,
                        mScrimVisibilitySupplier);

        tabContentManagerSupplier.set(tabContentManager);
        CompositorAnimationHandler.setTestingMode(true);
        mLayoutManagerChromePhone.init(
                mTabModelSelector,
                null,
                null,
                null,
                mTopUiThemeColorProvider,
                new ObservableSupplierImpl<>(0));
    }

    /**
     * Simulates time so the animation updates.
     *
     * @param layoutManager The {@link LayoutManagerChrome} to update.
     * @param maxFrameCount The maximum number of frames to simulate before the motion ends.
     * @return Whether the maximum number of frames was enough for the {@link LayoutManagerChrome}
     *     to reach the end of the animations.
     */
    private boolean simulateTime(LayoutManagerChrome layoutManager, int maxFrameCount) {
        // Simulating time
        int frame = 0;
        long time = 0;
        final long dt = 16;
        while (layoutManager.onUpdate(time, dt) && frame < maxFrameCount) {
            time += dt;
            frame++;
        }
        return frame < maxFrameCount;
    }
}
