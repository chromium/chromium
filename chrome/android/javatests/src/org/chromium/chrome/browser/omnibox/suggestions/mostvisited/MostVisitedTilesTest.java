// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static org.hamcrest.core.IsEqual.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.endsWith;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;

import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.LayoutManager;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.components.omnibox.AutocompleteMatch.SuggestTile;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * Tests of the Most Visited Tiles.
 * TODO(ender): add keyboard navigation for MV tiles once we can use real AutocompleteController.
 * The TestAutocompleteController is unable to properly classify the synthetic OmniboxSuggestions
 * and issuing KEYCODE_ENTER results in no action.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MostVisitedTilesTest {
    // A dummy URL used in the Omnibox for factual correctness.
    // The MV tiles are meant to be shown when the user is currently on any website.
    // Note: since we use the TestAutocompleteController, this could be any string.
    private static final String START_PAGE_LOCATION = "/echo/start.html";
    private static final String SEARCH_QUERY = "related search query";
    private static final int MV_TILE_CAROUSEL_MATCH_POSITION = 1;

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private Profile mProfile;

    @Mock
    private AutocompleteController mController;

    @Captor
    private ArgumentCaptor<AutocompleteController.OnSuggestionsReceivedListener> mListener;

    private ChromeTabbedActivity mActivity;
    private LocationBarLayout mLocationBarLayout;

    private AutocompleteCoordinator mAutocomplete;
    private EmbeddedTestServer mTestServer;
    private Tab mTab;
    private SuggestionInfo<BaseCarouselSuggestionView> mCarousel;
    private String mStartUrl;
    private OmniboxTestUtils mOmnibox;

    private SuggestTile mTile1;
    private SuggestTile mTile2;
    private SuggestTile mTile3;

    @BeforeClass
    public static void setUpClass() throws Exception {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        AutocompleteControllerProvider.setControllerForTesting(mController);
        mActivity = sActivityTestRule.getActivity();
        mOmnibox = new OmniboxTestUtils(mActivity);
        mLocationBarLayout = mActivity.findViewById(R.id.location_bar);
        mAutocomplete = mLocationBarLayout.getAutocompleteCoordinator();
        mTab = mActivity.getActivityTab();
        mStartUrl = sActivityTestRule.getTestServer().getURL(START_PAGE_LOCATION);

        ChromeTabUtils.waitForInteractable(mTab);
        ChromeTabUtils.loadUrlOnUiThread(mTab, mStartUrl);
        ChromeTabUtils.waitForTabPageLoaded(mTab, null);

        // clang-format off
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAutocomplete.setAutocompleteProfile(mProfile);
        });
        // clang-format on

        verify(mController).addOnSuggestionsReceivedListener(mListener.capture());

        setUpSuggestionsToShow();
        focusOmniboxAndWaitForSuggestions();

        mCarousel = mOmnibox.findSuggestionWithType(OmniboxSuggestionUiType.TILE_NAVSUGGEST);
    }

    @After
    public void tearDown() {
        AutocompleteControllerProvider.setControllerForTesting(null);
    }

    /**
     * Initialize a small set of suggestions resembling what the user would see while visiting an
     * URL.
     */
    private void setUpSuggestionsToShow() {
        // Set up basic AutocompleteResult hosting a MostVisitedTiles suggestion.
        mTestServer = sActivityTestRule.getTestServer();
        mTile1 = new SuggestTile("About", new GURL(mTestServer.getURL("/echo/tile1.html")), false);
        mTile2 = new SuggestTile(
                "Happy Server", new GURL(mTestServer.getURL("/echo/tile2.html")), false);
        mTile3 = new SuggestTile(
                "Test Server", new GURL(mTestServer.getURL("/echo/tile3.html")), false);

        AutocompleteResult autocompleteResult = AutocompleteResult.fromCache(
                null, GroupsInfo.newBuilder().putGroupConfigs(1, SECTION_2_WITH_HEADER).build());
        AutocompleteMatchBuilder builder = new AutocompleteMatchBuilder();

        // First suggestion is the current content of the Omnibox.
        builder.setType(OmniboxSuggestionType.NAVSUGGEST);
        builder.setDisplayText(START_PAGE_LOCATION);
        builder.setUrl(new GURL(mStartUrl));
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        // Second suggestion is the MV Tiles.
        builder.setType(OmniboxSuggestionType.TILE_NAVSUGGEST);
        builder.setSuggestTiles(Arrays.asList(new SuggestTile[] {mTile1, mTile2, mTile3}));
        builder.setDeletable(true);
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        // Third suggestion - search query with a header.
        builder.setType(OmniboxSuggestionType.SEARCH_SUGGEST);
        builder.setDisplayText(SEARCH_QUERY);
        builder.setIsSearch(true);
        builder.setGroupId(1);
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        doAnswer(invocation -> {
            mListener.getValue().onSuggestionsReceived(autocompleteResult, mStartUrl, true);
            return null;
        })
                .when(mController)
                .startZeroSuggest(endsWith(START_PAGE_LOCATION), any(), anyInt(), any());
    }

    private void focusOmniboxAndWaitForSuggestions() {
        ChromeTabUtils.waitForInteractable(mTab);
        mOmnibox.requestFocus();
        mOmnibox.checkSuggestionsShown();
    }

    private void clickTileAtPosition(int position) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LayoutManager manager = mCarousel.view.getRecyclerViewForTest().getLayoutManager();
            Assert.assertTrue(position < manager.getItemCount());
            manager.scrollToPosition(position);
            View view = manager.findViewByPosition(position);
            Assert.assertNotNull(view);
            view.performClick();
        });
    }

    private void longClickTileAtPosition(int position) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LayoutManager manager = mCarousel.view.getRecyclerViewForTest().getLayoutManager();
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
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile2.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile3.url.getSpec()), null);

        // Note: the carousel does not wrap around.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile3.url.getSpec()), null);
    }

    @Test
    @MediumTest
    public void keyboardNavigation_highlightingPreviousTileUpdatesUrlBarText()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile2.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);

        // Note: the carousel does not wrap around.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);
    }

    @Test
    @MediumTest
    public void keyboardNavigation_highlightAlwaysStartsWithFirstElement()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile2.url.getSpec()), null);

        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        mOmnibox.checkText(equalTo(mTile3.url.getSpec()), null);

        // Move to the search suggestion skipping the header.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        mOmnibox.checkText(equalTo(SEARCH_QUERY), null);

        // Move back to the MV Tiles. Observe that the first element is again highlighted.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_UP);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_UP);
        mOmnibox.checkText(equalTo(mTile1.url.getSpec()), null);
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnFirstMVTile() throws Exception {
        clickTileAtPosition(0);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile1.url.getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnMiddleMVTile() throws Exception {
        clickTileAtPosition(1);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile2.url.getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_clickOnLastMVTile() throws Exception {
        clickTileAtPosition(2);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile3.url.getSpec());
    }

    @Test
    @MediumTest
    public void touchNavigation_deleteMostVisitedTile() throws Exception {
        final int tileToDelete = 2;
        ModalDialogManager manager = mAutocomplete.getModalDialogManagerForTest();
        longClickTileAtPosition(tileToDelete);

        // Wait for the delete dialog to come up...
        CriteriaHelper.pollUiThread(() -> {
            PropertyModel deleteDialog = manager.getCurrentDialogForTest();
            if (deleteDialog == null) return false;
            deleteDialog.get(ModalDialogProperties.CONTROLLER)
                    .onClick(deleteDialog, ModalDialogProperties.ButtonType.POSITIVE);
            return true;
        });

        // ... and go away.
        CriteriaHelper.pollUiThread(() -> { return manager.getCurrentDialogForTest() == null; });

        verify(mController, times(1))
                .deleteMatchElement(eq(MV_TILE_CAROUSEL_MATCH_POSITION), eq(tileToDelete));
    }

    @Test
    @MediumTest
    public void touchNavigation_dismissDeleteMostVisitedTile() throws Exception {
        ModalDialogManager manager = mAutocomplete.getModalDialogManagerForTest();
        longClickTileAtPosition(2);

        // Wait for the delete dialog to come up...
        CriteriaHelper.pollUiThread(() -> {
            PropertyModel deleteDialog = manager.getCurrentDialogForTest();
            if (deleteDialog == null) return false;
            deleteDialog.get(ModalDialogProperties.CONTROLLER)
                    .onClick(deleteDialog, ModalDialogProperties.ButtonType.NEGATIVE);
            return true;
        });

        // ... and go away.
        CriteriaHelper.pollUiThread(() -> { return manager.getCurrentDialogForTest() == null; });
        verify(mController, never()).deleteMatchElement(anyInt(), anyInt());
    }
}
