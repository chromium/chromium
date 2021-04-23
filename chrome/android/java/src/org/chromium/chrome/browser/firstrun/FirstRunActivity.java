// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.fonts.FontPreloader;
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
     */
    public interface FirstRunActivityObserver {
        /** See {@link #onCreatePostNativeAndPoliciesPageSequence}. */
        void onCreatePostNativeAndPoliciesPageSequence(
                FirstRunActivity caller, Bundle freProperties);

        /** See {@link #acceptTermsOfService}. */
        void onAcceptTermsOfService(FirstRunActivity caller);

        /** See {@link #jumpToPage}. */
        void onJumpToPage(FirstRunActivity caller, int position);

        /** Called when First Run is completed. */
        void onUpdateCachedEngineName(FirstRunActivity caller);

        /** See {@link #abortFirstRunExperience}. */
        void onAbortFirstRunExperience(FirstRunActivity caller);

        /** See {@link #exitFirstRun()}. */
        void onExitFirstRun(FirstRunActivity caller);
    }

    // TODO(https://crbug.com/1196404): Replace with call into shared code once
    // https://crrev.com/c/2815659 lands.
    private static class ViewDrawBlocker {
        public static void blockViewDrawUntilReady(View view, Supplier<Boolean> viewReadySupplier) {
            view.getViewTreeObserver().addOnPreDrawListener(new OnPreDrawListener() {
                @Override
                public boolean onPreDraw() {
                    if (!viewReadySupplier.get()) return false;

                    view.getViewTreeObserver().removeOnPreDrawListener(this);
                    return true;
                }
            });
        }
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

    @Nullable
    private static FirstRunActivityObserver sObserver;

    private String mResultSignInAccountName;
    private boolean mResultIsDefaultAccount;
    private boolean mResultShowSignInSettings;

    private boolean mFlowIsKnown;
    private boolean mPostNativeAndPolicyPagesCreated;
    private boolean mNativeSideIsInitialized;
    private Set<FirstRunFragment> mPagesToNotifyOfNativeInit;
    private boolean mDeferredCompleteFRE;

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

    private final List<FirstRunPage> mPages = new ArrayList<>();
    private final List<Integer> mFreProgressStates = new ArrayList<>();

    private ViewPager2 mPager;

    /**
     * The pager adapter, which provides the pages to the view pager widget.
     */
    private FirstRunPagerAdapter mPagerAdapter;

    /**
     * Defines a sequence of pages to be shown (depending on parameters etc).
     */
    private void createPageSequence() {
        mPages.add(shouldCreateEnterpriseCctTosPage()
                        ? new TosAndUmaFirstRunFragmentWithEnterpriseSupport.Page()
                        : new ToSAndUMAFirstRunFragment.Page());
        mFreProgressStates.add(FRE_PROGRESS_WELCOME_SHOWN);
        // Other pages will be created by createPostNativeAndPoliciesPageSequence() after
        // native and policy service have been initialized.
    }

    private boolean shouldCreateEnterpriseCctTosPage() {
        // TODO(crbug.com/1111490): Revisit case when #shouldSkipWelcomePage = true.
        //  If the client has already accepted ToS (FirstRunStatus#shouldSkipWelcomePage), do not
        //  use the subclass ToSAndUmaCCTFirstRunFragment. Instead, use the base class
        //  (ToSAndUMAFirstRunFragment) which simply shows a loading spinner while waiting for
        //  native to be loaded.
        return mLaunchedFromCCT && !FirstRunStatus.shouldSkipWelcomePage();
    }

    /**
     * Create the page sequence which requires native initialized, and policies loaded if any
     * on-device policies may exists.
     *
     * @see #areNativeAndPoliciesInitialized()
     */
    private void createPostNativeAndPoliciesPageSequence() {
        // Note: Can't just use POST_NATIVE_SETUP_NEEDED for the early return, because this
        // populates |mPages| which needs to be done even if onNativeDependenciesFullyInitialized()
        // was performed in a previous session.
        if (mPostNativeAndPolicyPagesCreated) return;

        assert areNativeAndPoliciesInitialized();
        mFirstRunFlowSequencer.onNativeAndPoliciesInitialized(mFreProperties);

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
            mPages.add(SyncConsentFirstRunFragment::new);
            mFreProgressStates.add(FRE_PROGRESS_SIGNIN_SHOWN);
            notifyAdapter = true;
        }

        if (notifyAdapter && mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativeAndPolicyPagesCreated = true;

        if (sObserver != null) {
            sObserver.onCreatePostNativeAndPoliciesPageSequence(
                    FirstRunActivity.this, mFreProperties);
        }
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
        mPager = new ViewPager2(this);

        // Disable swipe gesture.
        mPager.setUserInputEnabled(false);

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
        ViewDrawBlocker.blockViewDrawUntilReady(
                findViewById(android.R.id.content), () -> mPages.size() > 0);

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                mFlowIsKnown = true;
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                mFreProperties = freProperties;
                createPageSequence();
                if (areNativeAndPoliciesInitialized()) {
                    createPostNativeAndPoliciesPageSequence();
                }

                if (mPages.size() == 0) {
                    completeFirstRunExperience();
                    return;
                }

                mPagerAdapter = new FirstRunPagerAdapter(FirstRunActivity.this, mPages);
                mPager.setAdapter(mPagerAdapter);

                if (areNativeAndPoliciesInitialized()) {
                    skipPagesIfNecessary();
                }

                recordFreProgressHistogram(mFreProgressStates.get(0));
                long inflationCompletion = SystemClock.elapsedRealtime();
                RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.FirstFragmentInflatedV2",
                        inflationCompletion - mIntentCreationElapsedRealtimeMs);
                getFirstRunAppRestrictionInfo().getCompletionElapsedRealtimeMs(
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
    protected void performPostInflationStartup() {
        super.performPostInflationStartup();

        FontPreloader.getInstance().onPostInflationStartupFre();
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

    private void onNativeDependenciesFullyInitialized() {
        mNativeSideIsInitialized = true;
        if (mDeferredCompleteFRE) {
            completeFirstRunExperience();
            mDeferredCompleteFRE = false;
        } else if (mFlowIsKnown) {
            boolean doCreatePostPolicyPageSequence = areNativeAndPoliciesInitialized();
            // Note: If mFlowIsKnown is false, then we're not ready to create the post native page
            // sequence - in that case this will be done when onFlowIsKnown() gets called.
            if (doCreatePostPolicyPageSequence) {
                createPostNativeAndPoliciesPageSequence();
            }

            if (mPagesToNotifyOfNativeInit != null) {
                for (FirstRunFragment page : mPagesToNotifyOfNativeInit) {
                    page.onNativeInitialized();
                }
                mPagesToNotifyOfNativeInit = null;
            }

            if (doCreatePostPolicyPageSequence) {
                skipPagesIfNecessary();
            }
        }
    }

    @Override
    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {
        super.onPolicyLoadListenerAvailable(onDevicePolicyFound);

        if (areNativeAndPoliciesInitialized()) {
            createPostNativeAndPoliciesPageSequence();
            skipPagesIfNecessary();
        }
    }

    private boolean areNativeAndPoliciesInitialized() {
        return mNativeSideIsInitialized && mFlowIsKnown
                && this.getPolicyLoadListener().get() != null;
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

        // Multiple active FREs does not really make sense for the user. Once one is complete, the
        // others would become out of date. This approach turns out to be quite tricky to enforce
        // completely with just Android configuration, because of all the different ways the FRE
        // can be launched, especially when it is not launching a new task and another activity's
        // traits are used. So instead just finish any FRE that is not ourselves manually.
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof FirstRunActivity && activity != this) {
                // Simple finish call only works when in the same task.
                if (activity.getTaskId() == this.getTaskId()) {
                    activity.finish();
                } else {
                    ApiCompatibilityUtils.finishAndRemoveTask(activity);
                    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                        // On L ApiCompatibilityUtils.finishAndRemoveTask() sometimes fails. Try one
                        // last time, see crbug.com/781396 for origin of this approach.
                        if (!activity.isFinishing()) {
                            activity.finish();
                        }
                    }
                }
            }
        }
    }

    @Override
    public void onBackPressed() {
        // Terminate if we are still waiting for the native or for Android EDU / GAIA Child checks.
        if (mPagerAdapter == null) {
            abortFirstRunExperience();
            return;
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
        if (sObserver != null) sObserver.onAbortFirstRunExperience(this);
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
        if (sObserver != null) sObserver.onUpdateCachedEngineName(this);

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

        if (sObserver != null) sObserver.onExitFirstRun(this);
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
        return FirstRunUtils.didAcceptTermsOfService();
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

        if (sObserver != null) sObserver.onAcceptTermsOfService(this);

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
        if (sObserver != null) sObserver.onJumpToPage(this, position);

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
        if (position >= mPagerAdapter.getItemCount()) {
            completeFirstRunExperience();
            return false;
        }

        int oldPosition = mPager.getCurrentItem();
        mPager.setCurrentItem(position, false);

        // Set A11y focus if possible. See https://crbug.com/1094064 for more context.
        // The screen reader can lose focus when switching between pages with ViewPager2.
        FirstRunFragment currentFragment = mPagerAdapter.getFirstRunFragment(position);
        if (currentFragment != null) {
            currentFragment.setInitialA11yFocus();
            if (oldPosition > position) {
                // If the fragment is revisited through back press, reset its state.
                currentFragment.reset();
            }
        }
        return true;
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

    @VisibleForTesting
    public boolean isNativeSideIsInitializedForTest() {
        return mNativeSideIsInitialized;
    }

    @VisibleForTesting
    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }
}
