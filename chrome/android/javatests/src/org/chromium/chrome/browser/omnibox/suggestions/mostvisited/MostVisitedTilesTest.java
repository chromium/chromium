// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static org.hamcrest.core.IsEqual.equalTo;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_MOST_VISITED;

import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.LayoutManager;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Tests of the Most Visited Tiles. TODO(ender): add keyboard navigation for MV tiles once we can
 * use real AutocompleteController. The TestAutocompleteController is unable to properly classify
 * the synthetic OmniboxSuggestions and issuing KEYCODE_ENTER results in no action.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MostVisitedTilesTest {
    // A placeholder URL used in the Omnibox for factual correctness.
    // The MV tiles are meant to be shown when the user is currently on any website.
    // Note: since we use the TestAutocompleteController, this could be any string.
    private static final String START_PAGE_LOCATION = "/echo/start.html";
    private static final String SEARCH_QUERY = "related search query";
    private static final int MV_TILE_CAROUSEL_MATCH_POSITION = 1;
    private static final long MV_TILE_NATIVE_HANDLE = 0xfce2;

    public final @Rule ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;
    private @Mock AutocompleteController mController;
    private @Captor ArgumentCaptor<AutocompleteController.OnSuggestionsReceivedListener> mListener;

    private ChromeTabbedActivity mActivity;
    private LocationBarLayout mLocationBarLayout;

    private AutocompleteCoordinator mAutocomplete;
    private EmbeddedTestServer mTestServer;
    private Tab mTab;
    private SuggestionInfo<BaseCarouselSuggestionView> mCarousel;
    private String mStartUrl;
    private OmniboxTestUtils mOmnibox;

    private AutocompleteMatch mMatch1;
    private AutocompleteMatch mMatch2;
    private AutocompleteMatch mMatch3;

    @Before
    public void setUp() throws Exception {
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
        doReturn(mController).when(mAutocompleteControllerJniMock).getForProfile(any());

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mActivity = mActivityTestRule.getActivity();
        mOmnibox = new OmniboxTestUtils(mActivity);
        mLocationBarLayout = mActivity.findViewById(R.id.location_bar);
        mAutocomplete = mLocationBarLayout.getAutocompleteCoordinator();
        mTab = mActivity.getActivityTab();
        mStartUrl = mActivityTestRule.getTestServer().getURL(START_PAGE_LOCATION);

        ChromeTabUtils.waitForInteractable(mTab);
        ChromeTabUtils.loadUrlOnUiThread(mTab, mStartUrl);
        ChromeTabUtils.waitForTabPageLoaded(mTab, null);
        verify(mController).addOnSuggestionsReceivedListener(mListener.capture());

        setUpSuggestionsToShow();

        mCarousel = mOmnibox.findSuggestionWithType(OmniboxSuggestionUiType.TILE_NAVSUGGEST);
    }

    /**
     * Initialize a small set of suggestions resembling what the user would see while visiting an
     * URL.
     */
    private void setUpSuggestionsToShow() {
        // Set up basic AutocompleteResult hosting a MostVisitedTiles suggestion.
        mTestServer = mActivityTestRule.getTestServer();

        AutocompleteResult autocompleteResult =
                spy(
                        AutocompleteResult.fromCache(
                                null,
                                GroupsInfo.newBuilder()
                                        .putGroupConfigs(1, SECTION_2_WITH_HEADER)
                                        .putGroupConfigs(2, SECTION_MOST_VISITED)
                                        .build()));
        AutocompleteMatchBuilder builder = new AutocompleteMatchBuilder();

        // First suggestion is the current content of the Omnibox.
        builder.setType(OmniboxSuggestionType.NAVSUGGEST);
        builder.setDisplayText(START_PAGE_LOCATION);
        builder.setUrl(new GURL(mStartUrl));
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        // Next, 3 MV Tiles.
        builder.setType(OmniboxSuggestionType.TILE_MOST_VISITED_SITE)
                .setIsSearch(false)
                .setGroupId(2)
                .setUrl(new GURL(mTestServer.getURL("/echo/tile1.html")))
                .setDisplayText("Tile#1")
                .setDeletable(true);
        mMatch1 = builder.build();
        mMatch1.updateNativeObjectRef(MV_TILE_NATIVE_HANDLE);
        autocompleteResult.getSuggestionsList().add(mMatch1);

        builder.setUrl(new GURL(mTestServer.getURL("/echo/tile2.html"))).setDisplayText("Tile#2");

        mMatch2 = builder.build();
        mMatch2.updateNativeObjectRef(MV_TILE_NATIVE_HANDLE);
        autocompleteResult.getSuggestionsList().add(mMatch2);

        builder.setUrl(new GURL(mTestServer.getURL("/echo/tile3.html"))).setDisplayText("Tile#3");

        mMatch3 = builder.build();
        mMatch3.updateNativeObjectRef(MV_TILE_NATIVE_HANDLE);
        autocompleteResult.getSuggestionsList().add(mMatch3);

        builder.reset();

        // Third suggestion - search query with a header.
        builder.setType(OmniboxSuggestionType.SEARCH_SUGGEST);
        builder.setDisplayText(SEARCH_QUERY);
        builder.setIsSearch(true);
        builder.setGroupId(1);
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        doReturn(true).when(autocompleteResult).verifyCoherency(anyInt(), anyInt());

        mOmnibox.requestFocus();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mListener.getValue().onSuggestionsReceived(autocompleteResult, true);
                });
        mOmnibox.checkSuggestionsShown();
    }

    private void clickTileAtPosition(int position) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LayoutManager manager = mCarousel.view.getLayoutManager();
                    Assert.assertTrue(position < manager.getItemCount());
                    manager.scrollToPosition(position);
                    View view = manager.findViewByPosition(position);
                    Assert.assertNotNull(view);
                    view.performClick();
                });
    }

    private void longClickTileAtPosition(int position) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LayoutManager manager = mCarousel.view.getLayoutManager();
                    Assert.assertTrue(position < manager.getItemCount());
                    manager.scrollToPosition(position);
                    View view = manager.findViewByPosition(position);
                    Assert.assertNotNull(view);
                    view.performLongClick();
                });
    }

    @Test
    @MediumTest
    public void keyboardNavigation_highlightingNextTileUpdatesUrlBarText()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(mMatch1.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(mMatch2.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(mMatch3.getUrl().getSpec()), null);

        // Note: the carousel does not wrap around, and Tab takes user to the next suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(SEARCH_QUERY), null);
    }

    @Test
    @MediumTest
    public void keyboardNavigation_highlightingPreviousTileUpdatesUrlBarText()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(mMatch1.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(mMatch2.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB, KeyEvent.META_SHIFT_ON);
        mOmnibox.checkText(equalTo(mMatch1.getUrl().getSpec()), null);

        // Note: the carousel does not wrap around, and Shift-Tab takes user to the previous
        // suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB, KeyEvent.META_SHIFT_ON);
        mOmnibox.checkText(equalTo(START_PAGE_LOCATION), null);
    }

    @Test
    @MediumTest
    public void keyboardNavigation_highlightAlwaysStartsWithFirstElement()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(mMatch1.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(mMatch2.getUrl().getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_TAB);
        mOmnibox.checkText(equalTo(mMatch3.getUrl().getSpec()), null);

        // Move to the search suggestion skipping the header.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(SEARCH_QUERY), null);

        // Move back to the MV Tiles. Observe that the first element is again highlighted.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_UP);
        mOmnibox.checkText(equalTo(mMatch1.getUrl().getSpec()), null);
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnFirstMVTile() throws Exception {
        clickTileAtPosition(0);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mMatch1.getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnMiddleMVTile() throws Exception {
        clickTileAtPosition(1);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mMatch2.getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnLastMVTile() throws Exception {
        clickTileAtPosition(2);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mMatch3.getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_deleteMostVisitedTile() throws Exception {
        ModalDialogManager manager = mAutocomplete.getModalDialogManagerForTest();
        longClickTileAtPosition(2);
        // onTopResumedActivityChanged calls `hideSuggestions()` which may bump the number of times
        // `stop()` is called.
        verify(mController, atLeastOnce()).stop(/* clear?=*/ eq(false));

        // Wait for the delete dialog to come up...
        CriteriaHelper.pollUiThread(
                () -> {
                    PropertyModel deleteDialog = manager.getCurrentDialogForTest();
                    if (deleteDialog == null) return false;
                    deleteDialog
                            .get(ModalDialogProperties.CONTROLLER)
                            .onClick(deleteDialog, ModalDialogProperties.ButtonType.POSITIVE);
                    return true;
                });

        // ... and go away.
        CriteriaHelper.pollUiThread(
                () -> {
                    return manager.getCurrentDialogForTest() == null;
                });

        verify(mController, times(1)).deleteMatch(eq(mMatch3));
    }

    @Test
    @MediumTest
    public void touchNavigation_dismissDeleteMostVisitedTile() throws Exception {
        ModalDialogManager manager = mAutocomplete.getModalDialogManagerForTest();
        longClickTileAtPosition(2);

        // Wait for the delete dialog to come up...
        CriteriaHelper.pollUiThread(
                () -> {
                    PropertyModel deleteDialog = manager.getCurrentDialogForTest();
                    if (deleteDialog == null) return false;
                    deleteDialog
                            .get(ModalDialogProperties.CONTROLLER)
                            .onClick(deleteDialog, ModalDialogProperties.ButtonType.NEGATIVE);
                    return true;
                });

        // ... and go away.
        CriteriaHelper.pollUiThread(
                () -> {
                    return manager.getCurrentDialogForTest() == null;
                });
        verify(mAutocompleteControllerJniMock, never())
                .deleteMatchElement(anyLong(), anyInt(), anyInt());
    }
}
