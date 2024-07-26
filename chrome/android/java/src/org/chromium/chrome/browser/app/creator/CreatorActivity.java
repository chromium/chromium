// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.webfeed.CreatorIntentConstants;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

// import org.chromium.components.feed.proto.wire.FeedEntryPointSource;

/** Activity for the Creator Page. */
public class CreatorActivity extends SnackbarActivity {
    private ActivityWindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private CreatorActionDelegateImpl mCreatorActionDelegate;
    private ActivityTabProvider mActivityTabProvider;
    private ActivityLifecycleDispatcherImpl mLifecycleDispatcher;
    private UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier;
    private UnownedUserDataSupplier<ShareDelegate> mTabShareDelegateSupplier;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private Profile mProfile;

    private static class TabShareDelegateImpl extends ShareDelegateImpl {
        public TabShareDelegateImpl(
                BottomSheetController controller,
                ActivityLifecycleDispatcherImpl lifecycleDispatcher,
                ActivityTabProvider tabProvider,
                ObservableSupplierImpl tabModelSelectorProvider,
                ObservableSupplierImpl profileSupplier,
                ShareSheetDelegate delegate,
                boolean isCustomTab) {
            super(
                    controller,
                    lifecycleDispatcher,
                    tabProvider,
                    tabModelSelectorProvider,
                    profileSupplier,
                    delegate,
                    isCustomTab);
        }

        @Override
        public boolean isSharingHubEnabled() {
            return false;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        byte[] webFeedId =
                getIntent().getByteArrayExtra(CreatorIntentConstants.CREATOR_WEB_FEED_ID);
        String url = getIntent().getStringExtra(CreatorIntentConstants.CREATOR_URL);
        boolean following =
                getIntent().getBooleanExtra(CreatorIntentConstants.CREATOR_FOLLOWING, false);
        int entryPoint =
                getIntent()
                        .getIntExtra(
                                CreatorIntentConstants.CREATOR_ENTRY_POINT,
                                SingleWebFeedEntryPoint.OTHER);
        int mParentTabId =
                getIntent().getIntExtra(CreatorIntentConstants.CREATOR_TAB_ID, INVALID_TAB_ID);

        mActivityTabProvider = new ActivityTabProvider();
        mLifecycleDispatcher = new ActivityLifecycleDispatcherImpl(this);
        mShareDelegateSupplier = new ShareDelegateSupplier();
        mTabShareDelegateSupplier = new ShareDelegateSupplier();

        super.onCreate(savedInstanceState);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfile = getProfileProvider().getOriginalProfile();
        mProfileSupplier.set(mProfile);

        IntentRequestTracker intentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid = new ActivityWindowAndroid(this, false, intentRequestTracker);

        TabShareDelegateImpl tabshareDelegate =
                new TabShareDelegateImpl(
                        mBottomSheetController,
                        mLifecycleDispatcher,
                        mActivityTabProvider,
                        /* tabModelSelectProvider */ new ObservableSupplierImpl<>(),
                        mProfileSupplier,
                        new ShareDelegateImpl.ShareSheetDelegate(),
                        /* isCustomTab= */ false);
        mTabShareDelegateSupplier.set(tabshareDelegate);

        CreatorCoordinator coordinator =
                new CreatorCoordinator(
                        this,
                        webFeedId,
                        getSnackbarManager(),
                        mWindowAndroid,
                        mProfile,
                        url,
                        this::createWebContents,
                        this::createNewTab,
                        mTabShareDelegateSupplier,
                        entryPoint,
                        following,
                        this::showSignInInterstitial);

        mBottomSheetController = coordinator.getBottomSheetController();

        ShareDelegate shareDelegate =
                new ShareDelegateImpl(
                        mBottomSheetController,
                        mLifecycleDispatcher,
                        mActivityTabProvider,
                        /* tabModelSelectProvider */ new ObservableSupplierImpl<>(),
                        mProfileSupplier,
                        new ShareDelegateImpl.ShareSheetDelegate(),
                        /* isCustomTab= */ false);
        mShareDelegateSupplier.set(shareDelegate);
        mCreatorActionDelegate =
                new CreatorActionDelegateImpl(
                        this,
                        mProfile,
                        getSnackbarManager(),
                        coordinator,
                        mParentTabId,
                        mBottomSheetController);

        coordinator.queryFeedStream(mCreatorActionDelegate, mShareDelegateSupplier);

        setContentView(coordinator.getView());
        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setTitle("");

        // For this Activity, the home button in the action bar acts as the back button.
        getSupportActionBar().setHomeActionContentDescription(R.string.back);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        mWindowAndroid.destroy();
        mTabShareDelegateSupplier.destroy();
        mShareDelegateSupplier.destroy();
        super.onDestroy();
    }

    // This implements the CreatorWebContents interface.
    public WebContents createWebContents() {
        return WebContentsFactory.createWebContents(mProfile, true, false);
    }

    // This implements the CreatorOpenTab interface.
    public void createNewTab(LoadUrlParams params) {
        new ChromeAsyncTabLauncher(false).launchNewTab(params, TabLaunchType.FROM_LINK, null);
    }

    // This implements the SignInInterstitialInitiator interface.
    public void showSignInInterstitial() {
        mCreatorActionDelegate.showSignInInterstitial(
                SigninAccessPoint.CREATOR_FEED_FOLLOW, mBottomSheetController, mWindowAndroid);
    }
}
