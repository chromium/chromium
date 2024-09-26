// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.os.Build.VERSION_CODES;
import android.provider.Browser;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.LifecycleOwner;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.browser.history.HistoryManagerToolbar.InfoHeaderPref;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrarJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.widget.DateDividedAdapter;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewHolder;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;

/** Tests the History UI. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.APP_SPECIFIC_HISTORY})
public class HistoryUITest {
    private static final int PAGE_INCREMENT = 2;
    private static final String HISTORY_SEARCH_QUERY = "some page";

    @Rule public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;
    private HistoryManager mHistoryManager;
    private HistoryContentManager mContentManager;
    private RecyclerView mRecyclerView;
    private Activity mActivity;

    private HistoryItem mItem1;
    private HistoryItem mItem2;
    private int mHeight;
    private OnBackPressedDispatcher mOnBackPressedDispatcher;
    private LifecycleOwner mLifecycleOwner;

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private SigninManager mSigninManager;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private HistoryAdapter mMockAdapter;
    @Mock private PackageManager mPackageManager;
    @Mock private AppFilterCoordinator mAppFilterSheet;
    @Mock private ApplicationInfo mPackageAppInfo;

    public static Matcher<Intent> hasData(GURL uri) {
        return IntentMatchers.hasData(uri.getSpec());
    }

    @Before
    public void setUp() throws Exception {
        mHistoryProvider = new StubbedHistoryProvider();
        long timestamp = new Date().getTime();
        mItem1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mItem2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(mItem1);
        mHistoryProvider.addItem(mItem2);

        ProfileManager.setLastUsedProfileForTesting(mProfile);

        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);
        doReturn(1L).when(mMockLargeIconBridgeJni).init();
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        doReturn(mPrefService).when(mUserPrefsJni).get(mProfile);
        doReturn(true).when(mPrefService).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY);
        doReturn(true).when(mPrefService).getBoolean(HistoryManager.HISTORY_CLUSTERS_VISIBLE_PREF);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        doReturn(mSigninManager).when(mIdentityService).getSigninManager(mProfile);
        mJniMocker.mock(PrefChangeRegistrarJni.TEST_HOOKS, mPrefChangeRegistrarJni);
        IncognitoUtils.setEnabledForTesting(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mOnBackPressedDispatcher = activity.getOnBackPressedDispatcher();
                            mLifecycleOwner = activity;
                        });
        boolean isAppSpecificHistoryEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.APP_SPECIFIC_HISTORY);
        mHistoryManager =
                new HistoryManager(
                        mActivity,
                        true,
                        mSnackbarManager,
                        mProfile,
                        /* bottomSheetController= */ null,
                        /* Supplier<Tab>= */ null,
                        mHistoryProvider,
                        new HistoryUmaRecorder(),
                        /* clientPackageName= */ null,
                        /* shouldShowClearData= */ true,
                        /* launchedForApp= */ false,
                        /* showAppFilter= */ isAppSpecificHistoryEnabled);
        mContentManager = mHistoryManager.getContentManagerForTests();
        mAdapter = mContentManager.getAdapter();
        mRecyclerView = mContentManager.getRecyclerView();

        // Layout the recycler view with ample height so that we can measure how much height it
        // needs to fully display its initial set of items.
        mRecyclerView.measure(0, 0);
        mRecyclerView.layout(0, 0, 600, 1000);
        // Constrain the recycler view to only the height it needs and lay it out again.
        mHeight = mRecyclerView.getMeasuredHeight();
        layoutRecyclerView();

        // App-specific history always enables the privacy disclaimer header item.
        int expectedItemCount = 4 + (isAppSpecificHistoryEnabled ? 1 : 0);

        Assert.assertEquals(expectedItemCount, mAdapter.getItemCount());

        BackPressHelper.create(
                mLifecycleOwner,
                mOnBackPressedDispatcher,
                mHistoryManager,
                SecondaryActivity.HISTORY);
    }

    @Test
    @SmallTest
    public void testRemove_SingleItem() throws Exception {
        final HistoryItemView itemView = (HistoryItemView) getItemView(2);

        itemView.getRemoveButtonForTests().performClick();

        // Check that one item was removed.
        ShadowLooper.idleMainLooper();
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

        performMenuAction(R.id.selection_mode_delete_menu_id);

        // Check that all items were removed. The onChangedCallback should be called three times -
        // once for each item that is being removed and once for the removal of the header.
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
    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    @Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testPrivacyDisclaimers_SignedOut_Ash() {
        // With ASH enabled, the header for disclaimer text is always visible.
        Assert.assertEquals(2, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedIn() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        setHasOtherFormsOfBrowsingData(false);

        Assert.assertEquals(1, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedInSynced() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        setHasOtherFormsOfBrowsingData(false);

        Assert.assertEquals(1, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testPrivacyDisclaimers_SignedInSyncedAndOtherForms() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        setHasOtherFormsOfBrowsingData(true);

        Assert.assertEquals(2, mAdapter.getFirstGroupForTests().size());
    }

    @Test
    @SmallTest
    public void testOpenItem() throws Exception {
        clickItem(2);
        assertThat(
                shadowOf(mActivity).peekNextStartedActivity(),
                allOf(hasAction(equalTo(Intent.ACTION_VIEW)), hasData(mItem1.getUrl())));
    }

    @Test
    @SmallTest
    public void testOpenSelectedItems() throws Exception {
        toggleItemSelection(2);
        toggleItemSelection(3);

        performMenuAction(R.id.selection_mode_open_in_incognito);
        Intent intent = shadowOf(mActivity).getNextStartedActivity();

        assertThat(intent, hasData(mItem1.getUrl()));
        Assert.assertEquals(
                intent.getSerializableExtra(IntentHandler.EXTRA_ADDITIONAL_URLS),
                Arrays.asList(mItem2.getUrl().getSpec()));
    }

    @Test
    @SmallTest
    public void testOpenItemIntent() {
        Intent intent =
                mHistoryManager
                        .getContentManagerForTests()
                        .getOpenUrlIntent(mItem1.getUrl(), null, false);
        Assert.assertEquals(mItem1.getUrl().getSpec(), intent.getDataString());
        Assert.assertFalse(intent.hasExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB));
        Assert.assertFalse(intent.hasExtra(Browser.EXTRA_CREATE_NEW_TAB));
        Assert.assertEquals(
                PageTransition.AUTO_BOOKMARK,
                intent.getIntExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, -1));

        intent =
                mHistoryManager
                        .getContentManagerForTests()
                        .getOpenUrlIntent(mItem2.getUrl(), true, true);
        Assert.assertEquals(mItem2.getUrl().getSpec(), intent.getDataString());
        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        Assert.assertEquals(
                PageTransition.AUTO_BOOKMARK,
                intent.getIntExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, -1));
    }

    @Test
    @SmallTest
    public void testOnHistoryDeleted() throws Exception {
        toggleItemSelection(2);

        mHistoryProvider.removeItem(mItem1);

        mAdapter.onHistoryDeleted();

        // The selection should be cleared and the items in the adapter should be reloaded.
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(3, mAdapter.getItemCount());
    }

    @Test
    @SmallTest
    public void testSupervisedUser() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final HistoryItemView item = (HistoryItemView) getItemView(2);
        View itemRemoveButton = item.getRemoveButtonForTests();

        // First check the behaviour for non-supervised users.

        // The item's remove button is visible when there is no selection.
        Assert.assertEquals(View.VISIBLE, itemRemoveButton.getVisibility());

        toggleItemSelection(2);
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isVisible());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isEnabled());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isEnabled());
        // The item's remove button is invisible for non-supervised users when there is a selection.
        Assert.assertEquals(View.INVISIBLE, item.getRemoveButtonForTests().getVisibility());

        // Turn selection off and check the remove button is visible.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.VISIBLE, item.getRemoveButtonForTests().getVisibility());

        // Now check the behaviour for supervised users.
        signInToSupervisedAccount();

        // The item's remove button remains visible when there is no selection.
        Assert.assertEquals(View.VISIBLE, itemRemoveButton.getVisibility());

        // Incognito is hidden.
        toggleItemSelection(2);
        Assert.assertNull(toolbar.getItemById(R.id.selection_mode_open_in_incognito));

        // History deletion behaviour is unchanged from the non-supervised case.
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isEnabled());
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.INVISIBLE, item.getRemoveButtonForTests().getVisibility());

        // Make sure selection is no longer enabled.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.VISIBLE, item.getRemoveButtonForTests().getVisibility());

        signOut();
    }

    @Test
    @SmallTest
    public void testToolbarShadow() throws Exception {
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());

        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());

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

        performMenuAction(R.id.search_menu_id);

        // The selection should be cleared when a search is started.
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Select an item and assert that the search view is no longer showing.
        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());

        // Clear the selection and assert that the search view is showing again.
        toggleItemSelection(2);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Close the search view.
        Assert.assertTrue(mHistoryManager.getHandleBackPressChangedSupplier().get());
        toolbar.onSearchNavigationBack();
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());
    }

    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    @Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Test
    @SmallTest
    public void testSearch_AppFilterChipVisible() {
        mAdapter.setClearBrowsingDataButtonVisibilityForTest(false);
        DateDividedAdapter.ItemGroup headerGroup = mAdapter.getFirstGroupForTests();
        // Disclaimer header should be present.
        Assert.assertEquals(1, headerGroup.size());
        performMenuAction(R.id.search_menu_id);
        // Now only the header for 'Filter by app' chip is visible.
        Assert.assertEquals(1, headerGroup.size());
    }

    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    @Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Test
    @SmallTest
    public void testSearch_AppFilterChipEnabledWithNonEmptyAppResult() throws Exception {
        Assert.assertTrue(mHistoryProvider.isQueryAppsTriggered());
        mAdapter.setClearBrowsingDataButtonVisibilityForTest(false);
        mAdapter.setPrivacyDisclaimer();
        mContentManager.setPackageManagerForTesting(mPackageManager);
        performMenuAction(R.id.search_menu_id);

        // Verify the button starts disabled.
        Assert.assertFalse(isAppFilterButtonEnabled());

        // Verify the button remains disabled if the query app result is empty.
        var result = new ArrayList<String>();
        mAdapter.onQueryAppsComplete(result);
        Assert.assertFalse(isAppFilterButtonEnabled());

        // Verify the button becomes enabled if the app result is non-empty.
        final String app1 = "org.chromium.chrome.Ernie";
        final String app2 = "org.chromium.chrome.Bert";
        result.add(app1);
        result.add(app2);
        when(mPackageManager.getApplicationInfo(eq(app1), anyInt())).thenReturn(mPackageAppInfo);
        when(mPackageManager.getApplicationInfo(eq(app2), anyInt()))
                .thenThrow(NameNotFoundException.class);
        mAdapter.onQueryAppsComplete(result);
        Assert.assertTrue(isAppFilterButtonEnabled());
    }

    private boolean isAppFilterButtonEnabled() {
        return mAdapter.hasListHeader() && mAdapter.getAppFilterButtonForTest().isEnabled();
    }

    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    @Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Test
    @SmallTest
    public void testSearch_AppFilterSheet() {
        mContentManager.setPackageManagerForTesting(mPackageManager);
        mContentManager.setAppFilterSheetForTesting(mAppFilterSheet);

        performMenuAction(R.id.search_menu_id);

        var result = new ArrayList<String>();
        String appId1 = "org.chromium.chrome.ernie";
        String appId2 = "org.chromium.chrome.bert";
        result.add(appId1);
        result.add(appId2);
        mAdapter.onQueryAppsComplete(result);

        mContentManager.onAppFilterClicked();
        verify(mAppFilterSheet).openSheet(eq(null));

        // Verify ContentManager is updated with the selected app info.
        AppInfo selected = new AppInfo("Ernie", null, appId1);
        mContentManager.onAppUpdated(selected);
        Assert.assertEquals(
                "The expected app 'Ernie' was not chosen",
                mContentManager.getAppInfoForTesting(),
                selected);

        AppInfo selected2 = new AppInfo("Bert", null, appId2);
        mContentManager.onAppUpdated(selected2);
        Assert.assertEquals(
                "The expected app 'Bert' was not chosen",
                mContentManager.getAppInfoForTesting(),
                selected2);

        // Revert to full history.
        mContentManager.onAppUpdated(null);
        Assert.assertEquals(
                "The history was not reverted to full",
                mContentManager.getAppInfoForTesting(),
                null);
    }

    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    @Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Test
    @SmallTest
    public void testAppInfoCache() throws Exception {
        var appInfoCache = mContentManager.getAppInfoCache();
        appInfoCache.setPackageManagerForTesting(mPackageManager);
        final String app1 = "org.chromium.chrome.AwesomeApp";
        when(mPackageManager.getApplicationInfo(eq(app1), anyInt())).thenReturn(mPackageAppInfo);

        AppInfo appInfo1 = appInfoCache.get(app1);
        verify(mPackageManager).getApplicationInfo(eq(app1), anyInt());
        clearInvocations(mPackageManager);

        // Get the info for the same app -> verify the cached item is returned, without
        // calling the system API again.
        Assert.assertEquals(appInfo1, appInfoCache.get(app1));
        verify(mPackageManager, never()).getApplicationInfo(eq(app1), anyInt());
        clearInvocations(mPackageManager);

        // Verify that a call with a non-existent app ID returns null.
        final String app2 = "org.chromium.chrome.UninstalledApp";
        when(mPackageManager.getApplicationInfo(eq(app2), anyInt()))
                .thenThrow(NameNotFoundException.class);
        AppInfo appInfo2 = appInfoCache.get(app2);
        Assert.assertFalse("Bad appId should return invalid AppInfo", appInfo2.isValid());
        clearInvocations(mPackageManager);

        // Verify that a call with the same non-exisitent app ID won't invoke the system API again.
        appInfo2 = appInfoCache.get(app2);
        verify(mPackageManager, never()).getApplicationInfo(eq(app2), anyInt());
        Assert.assertFalse("Bad appID should return invalid AppInfo again", appInfo2.isValid());
    }

    @Test
    @SmallTest
    public void testSearchViewDismissedByBackPress() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        View toolbarSearchView = toolbar.getSearchViewForTests();
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());

        performMenuAction(R.id.search_menu_id);

        // Select an item and assert that the search view is still not showing.
        toggleItemSelection(2);
        Assert.assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());

        // Press back press to unselect item and the search view is showing again.
        var backPressRecorder =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.SecondaryActivity", SecondaryActivity.HISTORY);
        Assert.assertTrue(mHistoryManager.getHandleBackPressChangedSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(mOnBackPressedDispatcher::onBackPressed);
        Assert.assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());
        backPressRecorder.assertExpected();

        // Press back to close the search view.
        var backPressRecorder2 =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.SecondaryActivity", SecondaryActivity.HISTORY);
        Assert.assertTrue(mHistoryManager.getHandleBackPressChangedSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(mOnBackPressedDispatcher::onBackPressed);
        Assert.assertEquals(View.GONE, toolbarShadow.getVisibility());
        Assert.assertEquals(View.GONE, toolbarSearchView.getVisibility());
        backPressRecorder2.assertExpected();
    }

    @Test
    @SmallTest
    public void testToggleInfoMenuItem() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);

        // Not signed in
        Assert.assertFalse(infoMenuItem.isVisible());
        DateDividedAdapter.ItemGroup headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(1, headerGroup.size());

        // Signed in but not synced and history has items. The info button should be hidden.
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        setHasOtherFormsOfBrowsingData(false);
        toolbar.onSignInStateChange();
        Assert.assertFalse(infoMenuItem.isVisible());

        // Signed in, synced, has other forms and has items
        // Privacy disclaimers should be shown by default
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue(infoMenuItem.isVisible());
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, headerGroup.size());

        // Toggle Info Menu Item to off
        mHistoryManager.onMenuItemClick(infoMenuItem);
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(1, headerGroup.size());

        // Toggle Info Menu Item to on
        mHistoryManager.onMenuItemClick(infoMenuItem);
        headerGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, headerGroup.size());
    }

    @Test
    @SmallTest
    public void testInfoIcon_OtherFormsOfBrowsingData() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue("Info icon should be visible.", infoMenuItem.isVisible());

        // Hide disclaimers to simulate setup for https://crbug.com/1071468.
        mHistoryManager.onMenuItemClick(infoMenuItem);
        Assert.assertFalse(
                "Privacy disclaimers should be hidden.",
                mHistoryManager
                        .getContentManagerForTests()
                        .getShouldShowPrivacyDisclaimersIfAvailable());

        // Simulate call indicating there are not other forms of browsing data.
        setHasOtherFormsOfBrowsingData(false);
        layoutRecyclerView();
        Assert.assertFalse("Info menu item should be hidden.", infoMenuItem.isVisible());

        // Simulate call indicating there are other forms of browsing data.
        setHasOtherFormsOfBrowsingData(true);
        Assert.assertTrue("Info menu item should bre visible.", infoMenuItem.isVisible());
    }

    @Test
    @SmallTest
    public void testInfoHeaderInSearchMode() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        final MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);

        // Sign in and set has other forms of browsing data to true.
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        setHasOtherFormsOfBrowsingData(true);

        toolbar.onSignInStateChange();
        mAdapter.onSignInStateChange();

        ShadowLooper.idleMainLooper();
        DateDividedAdapter.ItemGroup firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertTrue(infoMenuItem.isVisible());
        Assert.assertTrue(mAdapter.hasListHeader());
        Assert.assertEquals(2, firstGroup.size());

        // Enter search mode
        performMenuAction(R.id.search_menu_id);

        ShadowLooper.idleMainLooper();
        firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertFalse(infoMenuItem.isVisible());
        // The first group should be the history item group from SetUp()
        Assert.assertFalse(mAdapter.hasListHeader());
        Assert.assertEquals(3, firstGroup.size());
    }

    @Test
    @SmallTest
    public void testSearch_NotFound() {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();

        // Enter search mode
        performMenuAction(R.id.search_menu_id);
        EditText searchText = toolbar.findViewById(R.id.search_text);
        searchText.setText(HISTORY_SEARCH_QUERY);
        layoutRecyclerView();

        TextView emptyText =
                mHistoryManager.getSelectableListLayout().findViewById(R.id.empty_state_text_title);
        assertTrue(emptyText.getText().toString().startsWith("Can’t find that page."));
        assertNotNull(
                mHistoryManager.getSelectableListLayout().findViewById(R.id.empty_state_icon));
    }

    @Test
    @SmallTest
    public void testAppSpecificToolbar() throws Exception {
        final String appId = "org.chromium.app.AwesomeApp";
        when(mPackageManager.getApplicationInfo(eq(appId), anyInt())).thenReturn(mPackageAppInfo);
        mHistoryManager =
                new HistoryManager(
                        mActivity,
                        true,
                        mSnackbarManager,
                        mProfile,
                        /* bottomSheetController= */ null,
                        /* Supplier<Tab>= */ null,
                        mHistoryProvider,
                        new HistoryUmaRecorder(),
                        appId,
                        true,
                        true,
                        false);
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        Assert.assertNull(toolbar.getItemById(R.id.close_menu_id));
        Assert.assertEquals(
                toolbar.getNavigationButtonForTests(), NavigationButton.NORMAL_VIEW_BACK);

        Resources res = mActivity.getResources();
        Assert.assertEquals(
                "Open in new Chrome text menu is wrong",
                toolbar.getItemById(R.id.selection_mode_open_in_new_tab).getTitle(),
                res.getString(R.string.history_open_in_chrome));
        Assert.assertEquals(
                "Open in new Incognito Chrome menu text is wrong",
                toolbar.getItemById(R.id.selection_mode_open_in_incognito).getTitle(),
                res.getString(R.string.history_open_in_incognito_chrome));
    }

    @Test
    @SmallTest
    public void testAppSpecificToolbarHeaderStateNotPersisted() throws Exception {
        final String appId = "org.chromium.app.AwesomeApp";
        when(mPackageManager.getApplicationInfo(eq(appId), anyInt())).thenReturn(mPackageAppInfo);
        mHistoryManager =
                new HistoryManager(
                        mActivity,
                        true,
                        mSnackbarManager,
                        mProfile,
                        /* bottomSheetController= */ null,
                        /* Supplier<Tab>= */ null,
                        mHistoryProvider,
                        new HistoryUmaRecorder(),
                        appId,
                        true,
                        true,
                        false);
        InfoHeaderPref headerPref = mHistoryManager.getInfoHeaderPrefForTests();
        Assert.assertFalse(headerPref.isVisible());
        HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        MenuItem infoMenuItem = toolbar.getItemById(R.id.info_menu_id);
        mHistoryManager.onMenuItemClick(infoMenuItem);

        // Verify that the toggled state is not persisted to preference storage.
        Assert.assertFalse(headerPref.isVisible());
    }

    @Test
    @SmallTest
    public void testInvisibleHeader() {
        Assert.assertTrue(mAdapter.hasListHeader());

        // Not sign in and set clear browsing data button to invisible
        mAdapter.setClearBrowsingDataButtonVisibilityForTest(false);
        mAdapter.setPrivacyDisclaimer();

        DateDividedAdapter.ItemGroup firstGroup = mAdapter.getFirstGroupForTests();
        Assert.assertFalse(mAdapter.hasListHeader());
        Assert.assertEquals(3, firstGroup.size());
    }

    @Test
    @SmallTest
    @Ignore // See https://crbug.com/1358628
    public void testCopyLink() {
        final ClipboardManager clipboardManager =
                (ClipboardManager) mActivity.getSystemService(Context.CLIPBOARD_SERVICE);
        Assert.assertNotNull(clipboardManager);
        clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));
        // Clear the clipboard to make sure we start with a clean state.

        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();

        // Check that the copy link item is visible when one item is selected.
        toggleItemSelection(2);
        Assert.assertTrue(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());

        // Check that link is copied to the clipboard.
        performMenuAction(R.id.selection_mode_copy_link);
        Assert.assertEquals(mItem1.getUrl().getSpec(), clipboardManager.getText());

        // Check that the copy link item is not visible when more than one item is selected.
        toggleItemSelection(2);
        toggleItemSelection(3);
        Assert.assertFalse(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());
    }

    @Test
    @SmallTest
    public void testScrollToLoadEnabled() {
        // Reduce the height available to the recycler view to less than it needs so that scrolling
        // has an effect.
        mHeight--;
        layoutRecyclerView();

        mHistoryProvider.setPaging(PAGE_INCREMENT);
        long timestamp = new Date().getTime();
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(2, --timestamp));
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(3, --timestamp));
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(4, --timestamp));
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(5, --timestamp));
        int itemCount = mAdapter.getItemCount();
        // Trigger a reload of items so that the adapter sees that there are now more to load.
        mAdapter.startLoadingItems();

        scrollRecyclerViewToBottom();

        Assert.assertEquals(
                PAGE_INCREMENT + " more Items should be loaded",
                mAdapter.getItemCount(),
                itemCount + PAGE_INCREMENT);
        itemCount = mAdapter.getItemCount();

        scrollRecyclerViewToBottom();
        Assert.assertEquals(
                PAGE_INCREMENT + " more Items should be loaded",
                mAdapter.getItemCount(),
                itemCount + PAGE_INCREMENT);
    }

    @Test
    @SmallTest
    public void testScrollToLoadDisabled() throws Exception {
        mHistoryProvider.setPaging(PAGE_INCREMENT);
        HistoryContentManager.setScrollToLoadDisabledForTesting(true);
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(2, new Date().getTime()));
        mHistoryProvider.addItem(
                StubbedHistoryProvider.createHistoryItem(3, new Date().getTime() - 1));
        mHistoryProvider.addItem(
                StubbedHistoryProvider.createHistoryItem(4, new Date().getTime() - 2));
        mAdapter.startLoadingItems();
        int itemCount = mAdapter.getItemCount();
        scrollRecyclerViewToBottom();

        Assert.assertEquals(
                "Should not load more items into view after scroll",
                mAdapter.getItemCount(),
                itemCount);
        Assert.assertTrue(
                "Footer should be added to the end of the view", mAdapter.hasListFooter());
        Assert.assertEquals(
                "Footer group should contain one item", 1, mAdapter.getLastGroupForTests().size());

        // Verify the button is correctly displayed
        DateDividedAdapter.TimedItem item = mAdapter.getLastGroupForTests().getItemAt(0);
        MoreProgressButton button =
                (MoreProgressButton) ((DateDividedAdapter.FooterItem) item).getView();
        Assert.assertSame(
                "FooterItem view should be MoreProgressButton",
                mAdapter.getMoreProgressButtonForTest(),
                button);
        Assert.assertEquals(
                "State for the MPB should be button",
                button.getStateForTest(),
                MoreProgressButton.State.BUTTON);

        // Test click, should load more items
        button.findViewById(R.id.action_button).performClick();

        Assert.assertEquals(
                PAGE_INCREMENT + " more Items should be loaded",
                mAdapter.getItemCount(),
                itemCount + PAGE_INCREMENT);
    }

    private void toggleItemSelection(int position) {
        final SelectableItemView<HistoryItem> itemView = getItemView(position);
        itemView.performLongClick();
        layoutRecyclerView();
    }

    private void clickItem(int position) {
        getItemView(position).performClick();
    }

    @SuppressWarnings("unchecked")
    private SelectableItemView<HistoryItem> getItemView(int position) {
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(position);
        Assert.assertTrue(
                mostRecentHolder + " should be instance of SelectableItemViewHolder",
                mostRecentHolder instanceof SelectableItemViewHolder);
        return ((SelectableItemViewHolder<HistoryItem>) mostRecentHolder).getItemView();
    }

    private void setHasOtherFormsOfBrowsingData(final boolean hasOtherForms) {
        mAdapter.hasOtherFormsOfBrowsingData(hasOtherForms);
    }

    private void signInToSupervisedAccount() {
        // Sign in to account. Note that if supervised user is set before sign in, the supervised
        // user setting will be reset.
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        doReturn(true).when(mProfile).isChild();
        doReturn("ChildAccountSUID").when(mPrefService).getString(Pref.SUPERVISED_USER_ID);
        IncognitoUtils.setEnabledForTesting(false);
        mHistoryManager.getContentManagerForTests().onPreferenceChange();
        layoutRecyclerView();
    }

    private void signOut() {
        // Clear supervised user id.
        doReturn("").when(mPrefService).getString(Pref.SUPERVISED_USER_ID);
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());
    }

    private void performMenuAction(int menuItemId) {
        mHistoryManager.getToolbarForTests().getMenu().performIdentifierAction(menuItemId, 0);
        layoutRecyclerView();
    }

    private void layoutRecyclerView() {
        mRecyclerView.measure(0, 0);
        mRecyclerView.layout(0, 0, 600, mHeight);
    }

    private void scrollRecyclerViewToBottom() {
        mRecyclerView.scrollBy(0, mHeight);
    }
}
