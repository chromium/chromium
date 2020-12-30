// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Handles the First Run Experience sequences shown to the user launching Chrome for the first time.
 * It supports only a simple format of FRE:
 *   [Welcome]
 *   [Intro pages...]
 *   [Sign-in page]
 * The activity might be run more than once, e.g. 1) for ToS and sign-in, and 2) for intro.
 */
public class FirstRunActivity extends FirstRunActivityBase implements FirstRunPageDelegate {
    /**
     * Alerted about various events when FirstRunActivity performs them.
     * TODO(crbug.com/1114319): Rework and use a better testing setup.
     * */
    public interface FirstRunActivityObserver {
        /** See {@link #onFlowIsKnown}. */
        void onFlowIsKnown(Bundle freProperties);

        /** See {@link #acceptTermsOfService}. */
        void onAcceptTermsOfService();

        /** See {@link #jumpToPage}. */
        void onJumpToPage(int position);

        /** Called when First Run is completed. */
        void onUpdateCachedEngineName();

        /** See {@link #abortFirstRunExperience}. */
        void onAbortFirstRunExperience();

        /** See {@link #exitFirstRun()}. */
        void onExitFirstRun();
    }

    // UMA constants.
    private static final int SIGNIN_SETTINGS_DEFAULT_ACCOUNT = 0;
    private static final int SIGNIN_SETTINGS_ANOTHER_ACCOUNT = 1;
    private static final int SIGNIN_ACCEPT_DEFAULT_ACCOUNT = 2;
    private static final int SIGNIN_ACCEPT_ANOTHER_ACCOUNT = 3;
    private static final int SIGNIN_NO_THANKS = 4;
    private static final int SIGNIN_MAX = 5;

    private static final int FRE_PROGRESS_STARTED = 0;
    private static final int FRE_PROGRESS_WELCOME_SHOWN = 1;
    private static final int FRE_PROGRESS_DATA_SAVER_SHOWN = 2;
    private static final int FRE_PROGRESS_SIGNIN_SHOWN = 3;
    private static final int FRE_PROGRESS_COMPLETED_SIGNED_IN = 4;
    private static final int FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN = 5;
    private static final int FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN = 6;
    private static final int FRE_PROGRESS_MAX = 7;

    private static FirstRunActivityObserver sObserver;

    private String mResultSignInAccountName;
    private boolean mResultIsDefaultAccount;
    private boolean mResultShowSignInSettings;

    private boolean mFlowIsKnown;
    private boolean mPostNativePageSequenceCreated;
    private boolean mNativeSideIsInitialized;
    private Set<FirstRunFragment> mPagesToNotifyOfNativeInit;
    private boolean mDeferredCompleteFRE;

    private FirstRunViewPager mPager;

    private FirstRunFlowSequencer mFirstRunFlowSequencer;

    private Bundle mFreProperties;

    /**
     * Whether the first run activity was launched as a result of the user launching Chrome from the
     * Android app list.
     */
    private boolean mLaunchedFromChromeIcon;
    private boolean mLaunchedFromCCT;

    /**
     * {@link SystemClock} timestamp from when the FRE intent was initially created. This marks when
     * we first knew an FRE was needed, and is used as a reference point for various timing metrics.
     */
    private long mIntentCreationElapsedRealtimeMs;

    private final FirstRunAppRestrictionInfo mFirstRunAppRestrictionInfo;

    private final List<FirstRunPage> mPages = new ArrayList<>();
    private final List<Integer> mFreProgressStates = new ArrayList<>();

    /**
     * The pager adapter, which provides the pages to the view pager widget.
     */
    private FirstRunPagerAdapter mPagerAdapter;

    public FirstRunActivity() {
        mFirstRunAppRestrictionInfo = FirstRunAppRestrictionInfo.takeMaybeInitialized();
    }

    /**
     * Defines a sequence of pages to be shown (depending on parameters etc).
     */
    private void createPageSequence() {
        mPages.add(shouldCreateEnterpriseCctTosPage()
                        ? new TosAndUmaFirstRunFragmentWithEnterpriseSupport.Page()
                        : new ToSAndUMAFirstRunFragment.Page());
        mFreProgressStates.add(FRE_PROGRESS_WELCOME_SHOWN);
        // Other pages will be created by createPostNativePageSequence() after
        // native has been initialized.
    }

    private boolean shouldCreateEnterpriseCctTosPage() {
        // TODO(crbug.com/1111490): Revisit case when #shouldSkipWelcomePage = true.
        //  If the client has already accepted ToS (FirstRunStatus#shouldSkipWelcomePage), do not
        //  use the subclass ToSAndUmaCCTFirstRunFragment. Instead, use the base class
        //  (ToSAndUMAFirstRunFragment) which simply shows a loading spinner while waiting for
        //  native to be loaded.
        return mLaunchedFromCCT && !FirstRunStatus.shouldSkipWelcomePage();
    }

    private void createPostNativePageSequence() {
        // Note: Can't just use POST_NATIVE_SETUP_NEEDED for the early return, because this
        // populates |mPages| which needs to be done even even if onNativeInitialized() was
        // performed in a previous session.
        if (mPostNativePageSequenceCreated) return;
        mFirstRunFlowSequencer.onNativeInitialized(mFreProperties);

        boolean notifyAdapter = false;
        // An optional Data Saver page.
        if (mFreProperties.getBoolean(SHOW_DATA_REDUCTION_PAGE)) {
            mPages.add(new DataReductionProxyFirstRunFragment.Page());
            mFreProgressStates.add(FRE_PROGRESS_DATA_SAVER_SHOWN);
            notifyAdapter = true;
        }

        // An optional page to select a default search engine.
        if (mFreProperties.getBoolean(SHOW_SEARCH_ENGINE_PAGE)) {
            mPages.add(new DefaultSearchEngineFirstRunFragment.Page());
            mFreProgressStates.add(FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN);
            notifyAdapter = true;
        }

        // An optional sign-in page.
        if (mFreProperties.getBoolean(SHOW_SIGNIN_PAGE)) {
            mPages.add(SigninFirstRunFragment::new);
            mFreProgressStates.add(FRE_PROGRESS_SIGNIN_SHOWN);
            notifyAdapter = true;
        }

        if (notifyAdapter && mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativePageSequenceCreated = true;
    }

    @Override
    protected Bundle transformSavedInstanceStateForOnCreate(Bundle savedInstanceState) {
        // We pass null to Activity.onCreate() so that it doesn't automatically restore
        // the FragmentManager state - as that may cause fragments to be loaded that have
        // dependencies on native before native has been loaded (and then crash). Instead,
        // these fragments will be recreated manually by us and their progression restored
        // from |mFreProperties| which we still get from getSavedInstanceState() below.
        return null;
    }

    /**
     * Creates the content view for this activity.
     * The only thing subclasses can do is wrapping the view returned by super implementation
     * in some extra layout.
     */
    @CallSuper
    protected View createContentView() {
        mPager = new FirstRunViewPager(this);
        mPager.setId(R.id.fre_pager);
        mPager.setOffscreenPageLimit(3);
        return mPager;
    }

    @Override
    public void triggerLayoutInflation() {
        initializeStateFromLaunchData();
        RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.TriggerLayoutInflation",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

        setFinishOnTouchOutside(true);

        setContentView(createContentView());

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                mFlowIsKnown = true;
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                mFreProperties = freProperties;
                if (TextUtils.isEmpty(mResultSignInAccountName)) {
                    mResultSignInAccountName = mFreProperties.getString(
                            SigninFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO);
                }

                createPageSequence();
                if (mNativeSideIsInitialized) {
                    createPostNativePageSequence();
                }

                if (mPages.size() == 0) {
                    completeFirstRunExperience();
                    return;
                }

                mPagerAdapter = new FirstRunPagerAdapter(getSupportFragmentManager(), mPages);
                stopProgressionIfNotAcceptedTermsOfService();
                mPager.setAdapter(mPagerAdapter);

                if (mNativeSideIsInitialized) {
                    skipPagesIfNecessary();
                }

                if (sObserver != null) sObserver.onFlowIsKnown(mFreProperties);
                recordFreProgressHistogram(mFreProgressStates.get(0));
                long inflationCompletion = SystemClock.elapsedRealtime();
                RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.FirstFragmentInflatedV2",
                        inflationCompletion - mIntentCreationElapsedRealtimeMs);
                mFirstRunAppRestrictionInfo.getCompletionElapsedRealtimeMs(
                        restrictionsCompletion -> {
                            if (restrictionsCompletion > inflationCompletion) {
                                RecordHistogram.recordTimesHistogram(
                                        "MobileFre.FragmentInflationSpeed.FasterThanAppRestriction",
                                        restrictionsCompletion - inflationCompletion);
                            } else {
                                RecordHistogram.recordTimesHistogram(
                                        "MobileFre.FragmentInflationSpeed.SlowerThanAppRestriction",
                                        inflationCompletion - restrictionsCompletion);
                            }
                        });
            }
        };
        mFirstRunFlowSequencer.start();
        FirstRunStatus.setFirstRunTriggered(true);
        recordFreProgressHistogram(FRE_PROGRESS_STARTED);
        onInitialLayoutInflationComplete();

        RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.ActivityInflated",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        Runnable onNativeFinished = new Runnable() {
            @Override
            public void run() {
                if (isActivityFinishingOrDestroyed()) return;

                onNativeDependenciesFullyInitialized();
            }
        };
        TemplateUrlServiceFactory.get().runWhenLoaded(onNativeFinished);
    }

    public boolean isNativeSideIsInitializedForTest() {
        return mNativeSideIsInitialized;
    }

    private void onNativeDependenciesFullyInitialized() {
        mNativeSideIsInitialized = true;
        if (mDeferredCompleteFRE) {
            completeFirstRunExperience();
            mDeferredCompleteFRE = false;
        } else if (mFlowIsKnown) {
            // Note: If mFlowIsKnown is false, then we're not ready to create the post native page
            // sequence - in that case this will be done when onFlowIsKnown() gets called.
            createPostNativePageSequence();
            if (mPagesToNotifyOfNativeInit != null) {
                for (FirstRunFragment page : mPagesToNotifyOfNativeInit) {
                    page.onNativeInitialized();
                }
            }
            mPagesToNotifyOfNativeInit = null;
            skipPagesIfNecessary();
        }
    }

    // Activity:

    @Override
    public void onAttachFragment(Fragment fragment) {
        if (!(fragment instanceof FirstRunFragment)) return;

        FirstRunFragment page = (FirstRunFragment) fragment;
        if (mNativeSideIsInitialized) {
            page.onNativeInitialized();
            return;
        }

        if (mPagesToNotifyOfNativeInit == null) {
            mPagesToNotifyOfNativeInit = new HashSet<>();
        }
        mPagesToNotifyOfNativeInit.add(page);
    }

    @Override
    public void onRestoreInstanceState(Bundle state) {
        // Don't automatically restore state here. This is a counterpart to the override
        // of transformSavedInstanceStateForOnCreate() as the two need to be consistent.
        // The default implementation of this would restore the state of the views, which
        // would otherwise cause a crash in ViewPager used to manage fragments - as it
        // expects consistency between the states restored by onCreate() and this method.
        // Activity doesn't check for null on the parameter, so pass an empty bundle.
        super.onRestoreInstanceState(new Bundle());
    }

    @Override
    public void onStart() {
        super.onStart();
        stopProgressionIfNotAcceptedTermsOfService();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // As first run is complete, we no longer need FirstRunAppRestrictionInfo.
        mFirstRunAppRestrictionInfo.destroy();
    }

    @Override
    public void onBackPressed() {
        // Terminate if we are still waiting for the native or for Android EDU / GAIA Child checks.
        if (mPagerAdapter == null) {
            abortFirstRunExperience();
            return;
        }

        Object currentItem = mPagerAdapter.instantiateItem(mPager, mPager.getCurrentItem());
        if (currentItem instanceof FirstRunFragment) {
            FirstRunFragment page = (FirstRunFragment) currentItem;
            if (page.interceptBackPressed()) return;
        }

        if (mPager.getCurrentItem() == 0) {
            abortFirstRunExperience();
        } else {
            setCurrentItemForPager(mPager.getCurrentItem() - 1);
        }
    }

    // FirstRunPageDelegate:
    @Override
    public Bundle getProperties() {
        return mFreProperties;
    }

    @Override
    public void advanceToNextPage() {
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    @Override
    public void abortFirstRunExperience() {
        finish();

        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
        if (sObserver != null) sObserver.onAbortFirstRunExperience();
    }

    @Override
    public void completeFirstRunExperience() {
        if (!mNativeSideIsInitialized) {
            mDeferredCompleteFRE = true;
            return;
        }

        RecordHistogram.recordMediumTimesHistogram("MobileFre.FromLaunch.FreCompleted",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
        if (!TextUtils.isEmpty(mResultSignInAccountName)) {
            final int choice;
            if (mResultShowSignInSettings) {
                choice = mResultIsDefaultAccount ? SIGNIN_SETTINGS_DEFAULT_ACCOUNT
                                                 : SIGNIN_SETTINGS_ANOTHER_ACCOUNT;
            } else {
                choice = mResultIsDefaultAccount ? SIGNIN_ACCEPT_DEFAULT_ACCOUNT
                                                 : SIGNIN_ACCEPT_ANOTHER_ACCOUNT;
            }
            recordSigninChoiceHistogram(choice);
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_SIGNED_IN);
        } else {
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN);
        }

        FirstRunFlowSequencer.markFlowAsCompleted(
                mResultSignInAccountName, mResultShowSignInSettings);

        if (DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo()) {
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_ENABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(false);
            } else {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_DISABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(true);
            }
        }

        // Update the search engine name cached by the widget.
        SearchWidgetProvider.updateCachedEngineName();
        if (sObserver != null) sObserver.onUpdateCachedEngineName();

        launchPendingIntentAndFinish();
    }

    @Override
    public void exitFirstRun() {
        // This is important because the first run, when completed, will re-launch the original
        // intent. The re-launched intent will still need to know to avoid the FRE.
        FirstRunStatus.setFirstRunSkippedByPolicy(true);

        launchPendingIntentAndFinish();
    }

    private void launchPendingIntentAndFinish() {
        if (!sendFirstRunCompletePendingIntent()) {
            finish();
        } else {
            ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    boolean shouldFinish = false;
                    if (activity == FirstRunActivity.this) {
                        shouldFinish = (newState == ActivityState.STOPPED
                                || newState == ActivityState.DESTROYED);
                    } else {
                        shouldFinish = newState == ActivityState.RESUMED;
                    }
                    if (shouldFinish) {
                        finish();
                        ApplicationStatus.unregisterActivityStateListener(this);
                    }
                }
            });
        }

        if (sObserver != null) sObserver.onExitFirstRun();
    }

    @Override
    public void refuseSignIn() {
        recordSigninChoiceHistogram(SIGNIN_NO_THANKS);
        mResultSignInAccountName = null;
        mResultShowSignInSettings = false;
    }

    @Override
    public void acceptSignIn(String accountName, boolean isDefaultAccount, boolean openSettings) {
        mResultSignInAccountName = accountName;
        mResultIsDefaultAccount = isDefaultAccount;
        mResultShowSignInSettings = openSettings;
    }

    @Override
    public boolean didAcceptTermsOfService() {
        boolean result = FirstRunUtils.didAcceptTermsOfService();
        if (sObserver != null) sObserver.onAcceptTermsOfService();
        return result;
    }

    @Override
    public void acceptTermsOfService(boolean allowCrashUpload) {
        // If default is true then it corresponds to opt-out and false corresponds to opt-in.
        UmaUtils.recordMetricsReportingDefaultOptIn(!DEFAULT_METRICS_AND_CRASH_REPORTING);
        RecordHistogram.recordMediumTimesHistogram("MobileFre.FromLaunch.TosAccepted",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
        FirstRunUtils.acceptTermsOfService(allowCrashUpload);
        FirstRunStatus.setSkipWelcomePage(true);
        flushPersistentData();
        stopProgressionIfNotAcceptedTermsOfService();
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    /** Initialize local state from launch intent and from saved instance state. */
    private void initializeStateFromLaunchData() {
        if (getIntent() != null) {
            mLaunchedFromChromeIcon =
                    getIntent().getBooleanExtra(EXTRA_COMING_FROM_CHROME_ICON, false);
            mLaunchedFromCCT =
                    getIntent().getBooleanExtra(EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
            mIntentCreationElapsedRealtimeMs =
                    getIntent().getLongExtra(EXTRA_FRE_INTENT_CREATION_ELAPSED_REALTIME_MS, 0);
        }
    }

    /**
     * Transitions to a given page.
     * @param position A page index to transition to.
     * @return Whether the transition to a given page was allowed.
     */
    private boolean jumpToPage(int position) {
        if (sObserver != null) sObserver.onJumpToPage(position);

        if (!didAcceptTermsOfService()) {
            return position == 0;
        }
        if (!setCurrentItemForPager(position)) {
            return false;
        }
        recordFreProgressHistogram(mFreProgressStates.get(position));
        return true;
    }

    private boolean setCurrentItemForPager(int position) {
        if (position >= mPagerAdapter.getCount()) {
            completeFirstRunExperience();
            return false;
        }

        mPager.setCurrentItem(position, false);

        // Set A11y focus if possible. See https://crbug.com/1094064 for more context.
        // * Screen reader can lose focus when switching between pages with ViewPager;
        // * FragmentPagerStateAdapter is trying to limit access for the real fragment that we are
        // creating / created;
        // * Note that despite the function name and javadoc,
        // FragmentPagerStateAdapter#instantiateItem returns cached fragments when possible. This
        // should always be the case here as ViewPager#setCurrentItem will trigger instantiation if
        // needed. This function call to #instantiateItem is not creating new fragment here but
        // rather reading the ones already created.
        Object currentFragment = mPagerAdapter.instantiateItem(mPager, position);
        if (currentFragment instanceof FirstRunFragment) {
            ((FirstRunFragment) currentFragment).setInitialA11yFocus();
        }
        return true;
    }

    private void stopProgressionIfNotAcceptedTermsOfService() {
        if (mPagerAdapter == null) return;
        mPagerAdapter.setStopAtTheFirstPage(!didAcceptTermsOfService());
    }

    private void skipPagesIfNecessary() {
        if (mPagerAdapter == null) return;

        boolean shouldSkip = mPages.get(mPager.getCurrentItem()).shouldSkipPageOnCreate();
        while (shouldSkip) {
            if (!jumpToPage(mPager.getCurrentItem() + 1)) return;
            shouldSkip = mPages.get(mPager.getCurrentItem()).shouldSkipPageOnCreate();
        }
    }

    private void recordFreProgressHistogram(int state) {
        if (mLaunchedFromChromeIcon) {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.Progress.MainIntent", state, FRE_PROGRESS_MAX);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.Progress.ViewIntent", state, FRE_PROGRESS_MAX);
        }
    }

    private static void recordSigninChoiceHistogram(int signInChoice) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileFre.SignInChoice", signInChoice, SIGNIN_MAX);
    }

    @Override
    public void showInfoPage(@StringRes int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }

    @Override
    public FirstRunAppRestrictionInfo getFirstRunAppRestrictionInfo() {
        return mFirstRunAppRestrictionInfo;
    }

    @VisibleForTesting
    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }
}
