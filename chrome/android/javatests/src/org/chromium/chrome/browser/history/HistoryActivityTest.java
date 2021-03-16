// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.Intents.times;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.view.MenuItem;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryTestUtils.TestObserver;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.widget.DateDividedAdapter;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewHolder;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Date;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * Tests the {@link HistoryActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class HistoryActivityTest {
    @Rule
    public final IntentsTestRule<HistoryActivity> mActivityTestRule =
            new IntentsTestRule<>(HistoryActivity.class, false, false);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;
    private HistoryManager mHistoryManager;
    private RecyclerView mRecyclerView;
    private TestObserver mTestObserver;
    private PrefChangeRegistrar mPrefChangeRegistrar;

    private HistoryItem mItem1;
    private HistoryItem mItem2;

    @Before
    public void setUp() throws Exception {
        // Account not signed in by default with AccountManagerTestRule.
        // The clear browsing data header, one date view, and two
        // history item views should be shown, but the info header should not. We enforce a default
        // state because the number of headers shown depends on the signed-in state.

        mHistoryProvider = new StubbedHistoryProvider();

        Date today = new Date();
        long timestamp = today.getTime();
        mItem1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mItem2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(mItem1);
        mHistoryProvider.addItem(mItem2);

        HistoryManager.setProviderForTests(mHistoryProvider);

        launchHistoryActivity();
        HistoryTestUtils.setupHistoryTestHeaders(mAdapter, mTestObserver);

        Assert.assertEquals(4, mAdapter.getItemCount());
    }

    private void launchHistoryActivity() {
        HistoryActivity activity = mActivityTestRule.launchActivity(null);
        mHistoryManager = activity.getHistoryManagerForTests();
        mAdapter = mHistoryManager.getAdapterForTests();
        mTestObserver = new TestObserver();
        mHistoryManager.getSelectionDelegateForTests().addObserver(mTestObserver);
        mAdapter.registerAdapterDataObserver(mTestObserver);
        mRecyclerView = activity.findViewById(R.id.selectable_list_recycler_view);
    }

    @Test
    @SmallTest
    public void testRemove_SingleItem() throws Exception {
        int callCount = mTestObserver.onChangedCallback.getCallCount();
        final HistoryItemView itemView = (HistoryItemView) getItemView(2);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> itemView.getRemoveButtonForTests().performClick());

        // Check that one item was removed.
        mTestObserver.onChangedCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
        Assert.assertEquals(3, mAdapter.getItemCount());
        Assert.assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
        Assert.assertEquals(View.GONE, mHistoryManager.getEmptyViewForTests().getVisibility());
    }

    @Test
    @SmallTest
    public void testRemove_AllItems() throws Exception {
        toggleItemSelection(2);
        toggleItemSelection(3);

        int callCount = mTestObserver.onChangedCallback.getCallCount();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertTrue(
                    mHistoryManager.getToolbarForTests().getMenu().performIdentifierAction(
                            R.id.selection_mode_delete_menu_id, 0));
        });

        // Check that all items were removed. The onChangedCallback should be called three times -
        // once for each item that is being removed and once for the removal of the header.
        mTestObserver.onChangedCallback.waitForCallback(callCount, 3);
        Assert.assertEquals(0, mAdapter.getItemCount());
        Assert.assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, mRecyclerView.getVisibility());
        Assert.assertEquals(View.VISIBLE, mHistoryManager.getEmptyViewForTests().getVisibility());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedOut() {
        // The user is signed out by default.
        Assert.assertEquals(1, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedIn() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();

        setHasOtherFormsOfBrowsingData(false);

        Assert.assertEquals(1, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedInSynced() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();

        setHasOtherFormsOfBrowsingData(false);

        Assert.assertEquals(1, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedInSyncedAndOtherForms() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();

        setHasOtherFormsOfBrowsingData(true);

        Assert.assertEquals(2, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testOpenItem() throws Exception {
        intending(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem1.getUrl())))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        clickItem(2);

        intended(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem1.getUrl())), times(1));
    }

    @Test
    @SmallTest
    public void testOpenSelectedItems() throws Exception {
        // Stub out intent responses to prevent them from actually being sent.
        intending(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem1.getUrl())))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
        intending(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem2.getUrl())))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        toggleItemSelection(2);
        toggleItemSelection(3);

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertTrue(
                    mHistoryManager.getToolbarForTests().getMenu().performIdentifierAction(
                            R.id.selection_mode_open_in_incognito, 0));
        });

        intended(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem1.getUrl())), times(1));
        intended(allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem2.getUrl())), times(1));
    }

    @Test
    @SmallTest
    public void testOpenItemIntent() {
        Intent intent = mHistoryManager.getOpenUrlIntent(mItem1.getUrl(), null, false);
        Assert.assertEquals(mItem1.getUrl(), intent.getDataString());
        Assert.assertFalse(intent.hasExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB));
        Assert.assertFalse(intent.hasExtra(Browser.EXTRA_CREATE_NEW_TAB));
        Assert.assertEquals(PageTransition.AUTO_BOOKMARK,
                intent.getIntExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, -1));

        intent = mHistoryManager.getOpenUrlIntent(mItem2.getUrl(), true, true);
        Assert.assertEquals(mItem2.getUrl(), intent.getDataString());
        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        Assert.assertEquals(PageTransition.AUTO_BOOKMARK,
                intent.getIntExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, -1));
    }

    @Test
    @SmallTest
    public void testOnHistoryDeleted() throws Exception {
        toggleItemSelection(2);

        mHistoryProvider.removeItem(mItem1);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAdapter.onHistoryDeleted());

        // The selection should be cleared and the items in the adapter should be reloaded.
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(3, mAdapter.getItemCount());
    }

    @Test
    @SmallTest
    public void testSupervisedUser() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final HistoryItemView item = (HistoryItemView) getItemView(2);
        View itemRemoveButton = item.getRemoveButtonForTests();

        // The item's remove button is visible for non-supervised users when there is no selection.
        Assert.assertEquals(View.VISIBLE, itemRemoveButton.getVisibility());

        toggleItemSelection(2);
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isVisible());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isEnabled());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isEnabled());
        // The item's remove button is invisible for non-supervised users when there is a selection.
        Assert.assertEquals(View.INVISIBLE, item.getRemoveButtonForTests().getVisibility());

        // Turn selection off and check if remove button is visible.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.VISIBLE, item.getRemoveButtonForTests().getVisibility());

        signInToSupervisedAccount();

        Assert.assertEquals(View.GONE, item.getRemoveButtonForTests().getVisibility());
        toggleItemSelection(2);
        Assert.assertNull(toolbar.getItemById(R.id.selection_mode_open_in_incognito));
        Assert.assertNull(toolbar.getItemById(R.id.selection_mode_delete_menu_id));
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, item.getRemoveButtonForTests().getVisibility());

        // Make sure selection is no longer enabled.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, item.getRemoveButtonForTests().getVisibility());

        signOut();

        // Check that the item's remove button visibility is set correctly after signing out.
        Assert.assertEquals(View.VISIBLE, item.getRemoveButtonForTests().getVisibility());
    }

    @Test
    @SmallTest
    public void testToolbarShadow() throws Exception {
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());

        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.VISIBLE, toolbarShadow.getVisibility());

        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
    }

    @Test
    @SmallTest
    public void testSearchView() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        View toolbarSearchView = toolbar.getSearchViewForTests();
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());

        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());

        int callCount = mTestObserver.onSelectionCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0));

        // The selection should be cleared when a search is started.
        mTestObserver.onSelectionCallback.waitForCallback(callCount, 1);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Select an item and assert that the search view is no longer showing.
        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.VISIBLE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());

        // Clear the selection and assert that the search view is showing again.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Close the search view.
        TestThreadUtils.runOnUiThreadBlocking(() -> toolbar.onNavigationBack());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());
    }

    @Test
    @SmallTest
    public void testToggleInfoMenuItem() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);

        // Not signed in
        Assert.assertFalse(infoMenuItem.isVisible());
        DateDividedAdapter.ItemGroup headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(1, headerGroup.size());

        // Signed in but not synced and history has items. The info button should be hidden.
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setHasOtherFormsOfBrowsingData(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> toolbar.onSignInStateChange());
        Assert.assertFalse(infoMenuItem.isVisible());

        // Signed in, synced, has other forms and has items
        // Privacy disclaimers should be shown by default
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue(infoMenuItem.isVisible());
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, headerGroup.size());

        // Toggle Info Menu Item to off
        TestThreadUtils.runOnUiThreadBlocking(() -> mHistoryManager.onMenuItemClick(infoMenuItem));
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(1, headerGroup.size());

        // Toggle Info Menu Item to on
        TestThreadUtils.runOnUiThreadBlocking(() -> mHistoryManager.onMenuItemClick(infoMenuItem));
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, headerGroup.size());
    }

    @Test
    @SmallTest
    public void testInfoIcon_OtherFormsOfBrowsingData() throws ExecutionException {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue("Info icon should be visible.", infoMenuItem.isVisible());

        // Hide disclaimers to simulate setup for https://crbug.com/1071468.
        TestThreadUtils.runOnUiThreadBlocking(() -> mHistoryManager.onMenuItemClick(infoMenuItem));
        Assert.assertFalse("Privacy disclaimers should be hidden.",
                mHistoryManager.shouldShowInfoHeaderIfAvailable());

        // Simulate call indicating there are not other forms of browsing data.
        setHasOtherFormsOfBrowsingData(false);
        RecyclerViewTestUtils.waitForStableRecyclerView(mRecyclerView);
        Assert.assertFalse("Info menu item should be hidden.", infoMenuItem.isVisible());

        // Simulate call indicating there are other forms of browsing data.
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue("Info menu item should bre visible.", infoMenuItem.isVisible());
    }

    @Test
    @SmallTest
    public void testInfoHeaderInSearchMode() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);

        // Sign in and set has other forms of browsing data to true.
        int callCount = mTestObserver.onSelectionCallback.getCallCount();
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setHasOtherFormsOfBrowsingData(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            toolbar.onSignInStateChange();
            mAdapter.onSignInStateChange();
        });
        mTestObserver.onChangedCallback.waitForCallback(callCount, 1);
        DateDividedAdapter.ItemGroup firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(infoMenuItem.isVisible());
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, firstGroup.size());

        // Enter search mode
        callCount = mTestObserver.onSelectionCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0));

        mTestObserver.onSelectionCallback.waitForCallback(callCount, 1);
        firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertFalse(infoMenuItem.isVisible());
        // The first group should be the history item group from SetUp()
        Assert.assertFalse(mAdapter.hasListHeader());
        Assert.assertEquals(3, firstGroup.size());
    }

    @Test
    @SmallTest
    public void testInvisibleHeader() {
        Assert.assertTrue(mAdapter.hasListHeader());

        // Not sign in and set clear browsing data button to invisible
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAdapter.setClearBrowsingDataButtonVisibilityForTest(false);
            mAdapter.setPrivacyDisclaimer();
        });

        DateDividedAdapter.ItemGroup firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertFalse(mAdapter.hasListHeader());
        Assert.assertEquals(3, firstGroup.size());
    }

    @Test
    @SmallTest
    public void testCopyLink() throws Exception {
        // Allow DiskWrites temporarily in main thread to avoid
        // violation during copying under emulator environment.
        try (CloseableOnMainThread ignored = CloseableOnMainThread.StrictMode.allowDiskWrites()) {
            final ClipboardManager clipboardManager = TestThreadUtils.runOnUiThreadBlocking(() -> {
                ClipboardManager manager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                Assert.assertNotNull(manager);
                manager.setPrimaryClip(ClipData.newPlainText(null, ""));
                return manager;
            });
            // Clear the clipboard to make sure we start with a clean state.

            final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();

            // Check that the copy link item is visible when one item is selected.
            toggleItemSelection(2);
            Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());

            // Check that link is copied to the clipboard.
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> Assert.assertTrue(
                                    mHistoryManager.getToolbarForTests()
                                            .getMenu()
                                            .performIdentifierAction(
                                                    R.id.selection_mode_copy_link, 0)));
            CriteriaHelper.pollUiThread(
                    () -> Criteria.checkThat(mItem1.getUrl(), is(clipboardManager.getText())));

            // Check that the copy link item is not visible when more than one item is selected.
            toggleItemSelection(2);
            toggleItemSelection(3);
            Assert.assertFalse(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());
        }
    }

    // TODO(yolandyan): rewrite this with espresso
    private void toggleItemSelection(int position) throws Exception {
        int callCount = mTestObserver.onSelectionCallback.getCallCount();
        final SelectableItemView<HistoryItem> itemView = getItemView(position);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> { itemView.performLongClick(); });
        mTestObserver.onSelectionCallback.waitForCallback(callCount, 1);
    }

    private void clickItem(int position) throws Exception {
        final SelectableItemView<HistoryItem> itemView = getItemView(position);
        TestThreadUtils.runOnUiThreadBlocking(() -> itemView.performClick());
    }

    @SuppressWarnings("unchecked")
    private SelectableItemView<HistoryItem> getItemView(int position) {
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(position);
        Assert.assertTrue(mostRecentHolder + " should be instance of SelectableItemViewHolder",
                mostRecentHolder instanceof SelectableItemViewHolder);
        return ((SelectableItemViewHolder<HistoryItem>) mostRecentHolder).getItemView();
    }

    private void setHasOtherFormsOfBrowsingData(final boolean hasOtherForms) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAdapter.hasOtherFormsOfBrowsingData(hasOtherForms));
    }

    private void signInToSupervisedAccount() throws Exception {
        // Initialize PrefChangeRegistrar for test.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPrefChangeRegistrar = new PrefChangeRegistrar();
            mPrefChangeRegistrar.addObserver(Pref.ALLOW_DELETING_BROWSER_HISTORY, mTestObserver);
            mPrefChangeRegistrar.addObserver(Pref.INCOGNITO_MODE_AVAILABILITY, mTestObserver);
            IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .addSignInStateObserver(mTestObserver);
        });

        // Sign in to account. Note that if supervised user is set before sign in, the supervised
        // user setting will be reset.
        final CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        mTestObserver.onSigninStateChangedCallback.waitForCallback(
                0, 1, SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(coreAccountInfo, mAccountManagerTestRule.getCurrentSignedInAccount());

        // Wait for recycler view changes after sign in.
        CriteriaHelper.pollUiThread(() -> !mRecyclerView.isAnimating());

        // Set supervised user.
        int onPreferenceChangeCallCount = mTestObserver.onPreferenceChangeCallback.getCallCount();
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            UserPrefs.get(profile).setString(Pref.SUPERVISED_USER_ID, "ChildAccountSUID");
            return profile.isChild()
                    && !UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)
                    && !IncognitoUtils.isIncognitoModeEnabled();
        }));

        // Wait for preference change callbacks. One for ALLOW_DELETING_BROWSER_HISTORY and one for
        // INCOGNITO_MODE_AVAILABILITY.
        mTestObserver.onPreferenceChangeCallback.waitForCallback(onPreferenceChangeCallCount, 2);

        // Wait until animator finish removing history item delete icon
        // TODO(twellington): Figure out a better way to do this (e.g. listen for RecyclerView
        // data changes or add a testing callback)
        CriteriaHelper.pollUiThread(() -> !mRecyclerView.isAnimating());

        // Clean up PrefChangeRegistrar for test.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        });
    }

    private void signOut() throws Exception {
        // Clear supervised user id.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .setString(Pref.SUPERVISED_USER_ID, ""));

        // Sign out of account.
        int currentCallCount = mTestObserver.onSigninStateChangedCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> IdentityServicesProvider.get()
                                   .getSigninManager(Profile.getLastUsedRegularProfile())
                                   .signOut(SignoutReason.SIGNOUT_TEST));
        mTestObserver.onSigninStateChangedCallback.waitForCallback(currentCallCount, 1);
        Assert.assertNull(mAccountManagerTestRule.getCurrentSignedInAccount());

        // Remove observer
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> IdentityServicesProvider.get()
                                   .getSigninManager(Profile.getLastUsedRegularProfile())
                                   .removeSignInStateObserver(mTestObserver));
    }
}
