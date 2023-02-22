// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.fonts.FontPreloader;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.metrics.LowEntropySource;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.BitSet;
import java.util.List;
import java.util.function.BooleanSupplier;

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
        /** See {@link #createPostNativeAndPoliciesPageSequence}. */
        void onCreatePostNativeAndPoliciesPageSequence(FirstRunActivity caller);

        /** See {@link #acceptTermsOfService}. */
        void onAcceptTermsOfService(FirstRunActivity caller);

        /** See {@link #setCurrentItemForPager}. */
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

    private final BitSet mFreProgressStepsRecorded = new BitSet(MobileFreProgress.MAX);

    @Nullable
    private static FirstRunActivityObserver sObserver;

    private boolean mPostNativeAndPolicyPagesCreated;
    /** Use {@link Promise#isFulfilled()} to verify whether the native has been initialized. */
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();

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

    private boolean isFlowKnown() {
        return mFreProperties != null;
    }

    /** Creates first page and sets up adapter. Should result UI being shown on the screen. */
    private void createFirstPage() {
        BooleanSupplier showWelcomePage = () -> !FirstRunStatus.shouldSkipWelcomePage();
        if (FREMobileIdentityConsistencyFieldTrial.isEnabled()) {
            mPages.add(new FirstRunPage<>(SigninFirstRunFragment.class, showWelcomePage));
        } else {
            // TODO(crbug.com/1111490): Revisit during post-MVP.
            // There's an edge case where we accept the welcome page in the main app, abort the FRE,
            // then go through this CCT FRE again.
            mPages.add(shouldCreateEnterpriseCctTosPage()
                            ? new FirstRunPage<>(
                                    TosAndUmaFirstRunFragmentWithEnterpriseSupport.class,
                                    showWelcomePage)
                            : new FirstRunPage<>(ToSAndUMAFirstRunFragment.class, showWelcomePage));
        }
        mFreProgressStates.add(MobileFreProgress.WELCOME_SHOWN);
        mPagerAdapter = new FirstRunPagerAdapter(FirstRunActivity.this, mPages);
        mPager.setAdapter(mPagerAdapter);
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
        assert !mPostNativeAndPolicyPagesCreated;
        assert areNativeAndPoliciesInitialized();

        // Initialize SigninChecker, to kick off sign-in for child accounts as early as possible.
        //
        // TODO(b/245912657): explicitly sign in supervised users in {@link
        // SigninFirstRunMediator#handleContinueWithNative} rather than relying on SigninChecker.
        SigninCheckerProvider.get();

        mFirstRunFlowSequencer.updateFirstRunProperties(mFreProperties);

        BooleanSupplier showSearchEnginePromo =
                () -> mFreProperties.getBoolean(SHOW_SEARCH_ENGINE_PAGE);
        BooleanSupplier showSyncConsent = () -> mFreProperties.getBoolean(SHOW_SYNC_CONSENT_PAGE);

        // An optional page to select a default search engine.
        if (showSearchEnginePromo.getAsBoolean()) {
            mPages.add(new FirstRunPage<>(
                    DefaultSearchEngineFirstRunFragment.class, showSearchEnginePromo));
            mFreProgressStates.add(MobileFreProgress.DEFAULT_SEARCH_ENGINE_SHOWN);
        }

        // An optional sync consent page, the visibility of this page will be decided on the fly
        // according to the situation.
        mPages.add(new FirstRunPage<>(SyncConsentFirstRunFragment.class, showSyncConsent));
        mFreProgressStates.add(MobileFreProgress.SYNC_CONSENT_SHOWN);

        if (mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativeAndPolicyPagesCreated = true;

        if (sObserver != null) {
            sObserver.onCreatePostNativeAndPoliciesPageSequence(FirstRunActivity.this);
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

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
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
        // Generate trial group as early as possible to guarantee it's available by the time native
        // needs to register the synthetic trial group. See https://crbug.com/1295692 for details.
        FREMobileIdentityConsistencyFieldTrial.createFirstRunTrial();

        super.triggerLayoutInflation();

        initializeStateFromLaunchData();
        RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.TriggerLayoutInflation",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

        setFinishOnTouchOutside(true);

        setContentView(createContentView());
        if (FREMobileIdentityConsistencyFieldTrial.isEnabled() && mPagerAdapter == null) {
            // SigninFirstRunFragment doesn't use getProperties() and can be shown right away,
            // without waiting for FirstRunFlowSequencer.
            createFirstPage();
        } else {
            ViewDrawBlocker.blockViewDrawUntilReady(
                    findViewById(android.R.id.content), () -> mPages.size() > 0);
        }

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this, getChildAccountStatusSupplier()) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                assert freProperties != null;
                mFreProperties = freProperties;
                RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.ChildStatusAvailable",
                        SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

                onInternalStateChanged();

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
        recordFreProgressHistogram(MobileFreProgress.STARTED);
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
    protected void onFirstDrawComplete() {
        super.onFirstDrawComplete();

        FontPreloader.getInstance().onFirstDrawFre();
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
        mNativeInitializationPromise.fulfill(null);

        onInternalStateChanged();
    }

    @Override
    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {
        super.onPolicyLoadListenerAvailable(onDevicePolicyFound);
        RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.PoliciesLoaded",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

        onInternalStateChanged();
    }

    private void onInternalStateChanged() {
        if (!isFlowKnown()) {
            return;
        }

        if (mPagerAdapter == null) {
            createFirstPage();
        }

        if (!mPostNativeAndPolicyPagesCreated && areNativeAndPoliciesInitialized()) {
            createPostNativeAndPoliciesPageSequence();
        }

        if (areNativeAndPoliciesInitialized()) {
            skipPagesIfNecessary();
        }
    }

    private boolean areNativeAndPoliciesInitialized() {
        return mNativeInitializationPromise.isFulfilled() && isFlowKnown()
                && this.getPolicyLoadListener().get() != null;
    }

    // Activity:

    @Override
    public void onAttachFragment(Fragment fragment) {
        if (!(fragment instanceof FirstRunFragment)) return;

        FirstRunFragment page = (FirstRunFragment) fragment;
        // Delay notifying the child page until native and the TemplateUrlService are initialized.
        // Tracked by mNativeSideIsInitialized is ready. Otherwise if the next page handles
        // the default search engine, it will be missing dependencies. See https://crbug.com/1275950
        // for when this didn't work.
        if (mNativeInitializationPromise.isFulfilled()) {
            page.onNativeInitialized();
        } else {
            mNativeInitializationPromise.then((ignored) -> { page.onNativeInitialized(); });
        }
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
                    activity.finishAndRemoveTask();
                }
            }
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        // Terminate if we are still waiting for the native or for Android EDU / GAIA Child checks.
        if (!mPostNativeAndPolicyPagesCreated) {
            abortFirstRunExperience();
            return BackPressResult.SUCCESS;
        }

        mFirstRunFlowSequencer.updateFirstRunProperties(mFreProperties);

        int position = mPager.getCurrentItem() - 1;
        while (position > 0 && !mPages.get(position).shouldShow()) {
            --position;
        }

        if (position < 0) {
            abortFirstRunExperience();
        } else {
            setCurrentItemForPager(position);
        }
        return BackPressResult.SUCCESS;
    }

    // FirstRunPageDelegate:
    @Override
    public Bundle getProperties() {
        return mFreProperties;
    }

    @Override
    public boolean advanceToNextPage() {
        mFirstRunFlowSequencer.updateFirstRunProperties(mFreProperties);

        int position = mPager.getCurrentItem() + 1;
        while (position < mPagerAdapter.getItemCount() && !mPages.get(position).shouldShow()) {
            ++position;
        }
        if (!setCurrentItemForPager(position)) return false;

        recordFreProgressHistogram(mFreProgressStates.get(position));
        return true;
    }

    @Override
    public void abortFirstRunExperience() {
        finish();

        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
        if (sObserver != null) sObserver.onAbortFirstRunExperience(this);
    }

    @Override
    public void completeFirstRunExperience() {
        RecordHistogram.recordMediumTimesHistogram("MobileFre.FromLaunch.FreCompleted",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

        FirstRunFlowSequencer.markFlowAsCompleted();

        // LowEntropySource can't be used after the FRE has been completed.
        LowEntropySource.markFirstRunComplete();

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
    public boolean didAcceptTermsOfService() {
        return FirstRunUtils.didAcceptTermsOfService();
    }

    @Override
    public boolean isLaunchedFromCct() {
        return mLaunchedFromCCT;
    }

    @Override
    public void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        assert mNativeInitializationPromise.isFulfilled();

        // If default is true then it corresponds to opt-out and false corresponds to opt-in.
        UmaUtils.recordMetricsReportingDefaultOptIn(!DEFAULT_METRICS_AND_CRASH_REPORTING);
        RecordHistogram.recordMediumTimesHistogram("MobileFre.FromLaunch.TosAccepted",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
        FirstRunUtils.acceptTermsOfService(allowMetricsAndCrashUploading);
        FirstRunStatus.setSkipWelcomePage(true);
        flushPersistentData();

        if (sObserver != null) sObserver.onAcceptTermsOfService(this);
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

    private boolean setCurrentItemForPager(int position) {
        if (sObserver != null) sObserver.onJumpToPage(this, position);

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
        while (!mPages.get(mPager.getCurrentItem()).shouldShow() && advanceToNextPage()) {
        }
    }

    @Override
    public void recordFreProgressHistogram(@MobileFreProgress int state) {
        assert 0 <= state && state < MobileFreProgress.MAX;

        if (mFreProgressStepsRecorded.get(state)) return;

        mFreProgressStepsRecorded.set(state);
        if (mLaunchedFromChromeIcon) {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.Progress.MainIntent", state, MobileFreProgress.MAX);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.Progress.ViewIntent", state, MobileFreProgress.MAX);
        }
    }

    @Override
    public void recordNativePolicyAndChildStatusLoadedHistogram() {
        RecordHistogram.recordTimesHistogram(
                "MobileFre.FromLaunch.NativePolicyAndChildStatusLoaded",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
    }

    @Override
    public void recordNativeInitializedHistogram() {
        RecordHistogram.recordTimesHistogram("MobileFre.FromLaunch.NativeInitialized",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);
    }

    @Override
    public void showInfoPage(@StringRes int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }

    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return mNativeInitializationPromise;
    }

    @Override
    public boolean canUseLandscapeLayout() {
        return !getResources().getConfiguration().isLayoutSizeAtLeast(
                Configuration.SCREENLAYOUT_SIZE_LARGE);
    }

    @VisibleForTesting
    boolean hasPages() {
        return mPagerAdapter != null && mPagerAdapter.getItemCount() > 0;
    }

    @VisibleForTesting
    public FirstRunFragment getCurrentFragmentForTesting() {
        return mPagerAdapter.getFirstRunFragment(mPager.getCurrentItem());
    }

    @VisibleForTesting
    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }
}
