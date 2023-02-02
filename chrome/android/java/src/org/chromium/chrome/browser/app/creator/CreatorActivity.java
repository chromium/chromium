// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.feed.webfeed.CreatorIntentConstants;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

/**
 * Activity for the Creator Page.
 */
public class CreatorActivity extends SnackbarActivity {
    private ActivityWindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private ViewGroup mBottomSheetContainer;
    private CreatorActionDelegateImpl mCreatorActionDelegate;
    private ScrimCoordinator mScrim;
    private EphemeralTabCoordinator mEphemeralTabCoordinator;
    private ActivityTabProvider mActivityTabProvider;
    private ActivityLifecycleDispatcherImpl mLifecycleDispatcher;
    private UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier;
    private UnownedUserDataSupplier<ShareDelegate> mTabShareDelegateSupplier;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private Profile mProfile;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        byte[] mWebFeedId =
                getIntent().getByteArrayExtra(CreatorIntentConstants.CREATOR_WEB_FEED_ID);
        String mTitle = getIntent().getStringExtra(CreatorIntentConstants.CREATOR_TITLE);
        String mUrl = getIntent().getStringExtra(CreatorIntentConstants.CREATOR_URL);
        mActivityTabProvider = new ActivityTabProvider();
        mLifecycleDispatcher = new ActivityLifecycleDispatcherImpl(this);
        mShareDelegateSupplier = new ShareDelegateSupplier();
        mTabShareDelegateSupplier = new ShareDelegateSupplier();
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfile = Profile.getLastUsedRegularProfile();
        mProfileSupplier.set(mProfile);

        super.onCreate(savedInstanceState);
        IntentRequestTracker intentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid = new ActivityWindowAndroid(this, false, intentRequestTracker);
        CreatorCoordinator coordinator = new CreatorCoordinator(this, mWebFeedId,
                getSnackbarManager(), mWindowAndroid, mProfile, mTitle, mUrl,
                this::createWebContents, this::createNewTab, mTabShareDelegateSupplier);

        mBottomSheetController = coordinator.getBottomSheetController();
        ShareDelegate shareDelegate = new ShareDelegateImpl(mBottomSheetController,
                mLifecycleDispatcher, mActivityTabProvider,
                /* tabModelSelectProvider */ new ObservableSupplierImpl<>(), mProfileSupplier,
                new ShareDelegateImpl.ShareSheetDelegate(),
                /* isCustomTab */ false);
        mShareDelegateSupplier.set(shareDelegate);
        mCreatorActionDelegate =
                new CreatorActionDelegateImpl(this, mProfile, getSnackbarManager(), coordinator);
        coordinator.initFeedStream(mCreatorActionDelegate,
                HelpAndFeedbackLauncherImpl.getInstance(), mShareDelegateSupplier);

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
        mTabShareDelegateSupplier.destroy();
        mShareDelegateSupplier.destroy();
        super.onDestroy();
    }

    // This implements the CreatorWebContents interface.
    public WebContents createWebContents() {
        return WebContentsFactory.createWebContents(mProfile, true);
    }

    // This implements the CreatorOpenTab interface.
    public void createNewTab(LoadUrlParams params) {
        new TabDelegate(false).createNewTab(params, TabLaunchType.FROM_LINK, null);
    }
}
