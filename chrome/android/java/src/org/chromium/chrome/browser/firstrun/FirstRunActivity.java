// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fonts.FontPreloader;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.browser.ui.signin.DialogWhenLargeContentLayout;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.metrics.LowEntropySource;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;
import java.util.BitSet;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Handles the First Run Experience sequences shown to the user launching Chrome for the first time.
 * It supports only a simple format of FRE:
 *
 * <pre>
 *   [Welcome]
 *   [Intro pages...]
 *   [Sign-in page]
 * </pre>
 *
 * The activity might be run more than once, e.g. 1) for ToS and sign-in, and 2) for intro.
 */
public class FirstRunActivity extends FirstRunActivityBase implements FirstRunPageDelegate {

    /**
     * A simple page transformer for transitions between successive Fragment, aiming to be as close
     * as possible to inter-Activity transitions.
     */
    class FirstRunPageTransformer implements ViewPager2.PageTransformer {
        // The exiting page fades out, then tne entering page fades in. This is the alpha boundary
        // expressed as fraction of total animation duration.
        private static final float ALPHA_BOUNDARY_FRAC = 100f / 450f;

        // Absolute horizontal shift of a page at position 1 or -1 as fraction of screen width.
        private static final float MAX_X_SHIFT_FRACTION = 0.2f;

        // The direction in which content moves, and is opposite to page view change; thus if
        // transformPage() expects calls with increasing {@param position} for each {@param view},
        // then this would be negative.
        private int mDir = 1;

        public void setDirection(int dir) {
            mDir = dir;
        }

        @Override
        public void transformPage(View view, float position) {
            int pageWidth = view.getWidth();

            if (position <= -1 || position >= 1) { // [-Infinity,-1] or [1,+infinity]
                // Page is way off-screen to the left or right.
                view.setAlpha(0f);
            } else {
                // Position opposes direction of travel means page is entering; else exiting.
                boolean isEnter = mDir * position <= 0;
                // Value that linearly increases from 0 to 1 throughout transition.
                float progress = isEnter ? 1f - Math.abs(position) : Math.abs(position);
                // Full extent of motion: Start value if enter; final value if exit.
                float xShift = MAX_X_SHIFT_FRACTION * pageWidth * ((position < 0) ? -1f : 1f);
                // API idiosyncrasy: Assigning setTranslationX(0f) always leads to page shift:
                // * LTR: From right edge when `position` = 1 and fully seen when 0.
                // * RTL: From left edge when `position` = 1 and fully seen when 0.
                // Custom delta X translation is done by first counteracting this default shift:
                // * LTR: setTranslationX(-position * pageWidth + (custom delta X)).
                // * RTL: setTranslationX(position * pageWidth - (custom delta X)).
                // Since signs are simply opposite, we can thus initialize `x` to
                // `-position * pageWidth`, compute desired delta X assuming LTR, and then pass the
                // result (negated if RTL) to setTranslationX().
                float x = -position * pageWidth;

                if (isEnter) {
                    // Alpha: Wait for alpha boundary, then fade in.
                    float alphaProgress =
                            Math.max(
                                    0f,
                                    (progress - ALPHA_BOUNDARY_FRAC) / (1f - ALPHA_BOUNDARY_FRAC));
                    view.setAlpha(Interpolators.LEGACY_DECELERATE.getInterpolation(alphaProgress));
                    // `x` delta: Changes from `xShift` to 0.
                    x += (1f - Interpolators.EMPHASIZED.getInterpolation(progress)) * xShift;
                    // Place in front of page that's exiting.
                    view.setTranslationZ(1f);
                } else {
                    // Alpha: Fade out up to alpha boundary.
                    float alphaProgress = Math.min(progress / ALPHA_BOUNDARY_FRAC, 1f);
                    view.setAlpha(
                            1f - Interpolators.LEGACY_ACCELERATE.getInterpolation(alphaProgress));
                    // `x` delta: Changes from 0 to `xShift`.
                    x += Interpolators.EMPHASIZED.getInterpolation(progress) * xShift;
                    // Place behind page that's entering.
                    view.setTranslationZ(-1f);
                }

                view.setTranslationX((isRtl() ? -1f : 1f) * x);
            }
        }
    }

    /**
     * Alerted about various events when FirstRunActivity performs them. TODO(crbug.com/40710744):
     * Rework and use a better testing setup.
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

    private static final int TRANSITION_DELAY_MS = 450;

    private final BitSet mFreProgressStepsRecorded = new BitSet(MobileFreProgress.MAX);

    @Nullable private static FirstRunActivityObserver sObserver;

    private static boolean sIsAnimationDisabled;

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

    private boolean mLaunchedFromCct;

    /**
     * {@link SystemClock} timestamp from when the FRE intent was initially created. This marks when
     * we first knew an FRE was needed, and is used as a reference point for various timing metrics.
     */
    private long mIntentCreationElapsedRealtimeMs;

    private final List<FirstRunPage> mPages = new ArrayList<>();
    private final List<Integer> mFreProgressStates = new ArrayList<>();

    private FirstRunPageTransformer mPageTransformer;
    private ViewPager2 mPager;

    /** The pager adapter, which provides the pages to the view pager widget. */
    private FirstRunPagerAdapter mPagerAdapter;

    private boolean isFlowKnown() {
        return mFreProperties != null;
    }

    /** Creates first page and sets up adapter. Should result UI being shown on the screen. */
    private void createFirstPage() {
        BooleanSupplier showWelcomePage = () -> !FirstRunStatus.shouldSkipWelcomePage();
        mPages.add(new FirstRunPage<>(SigninFirstRunFragment.class, showWelcomePage));
        mFreProgressStates.add(MobileFreProgress.WELCOME_SHOWN);
        mPagerAdapter = new FirstRunPagerAdapter(FirstRunActivity.this, mPages);
        mPager.setAdapter(mPagerAdapter);
        mPageTransformer = new FirstRunPageTransformer();
        mPager.setPageTransformer(mPageTransformer);

        // Other pages will be created by createPostNativeAndPoliciesPageSequence() after
        // native and policy service have been initialized.
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
        // FullscreenSigninMediator#handleContinueWithNative} rather than relying on SigninChecker.
        SigninCheckerProvider.get(getProfileProviderSupplier().get().getOriginalProfile());

        mFirstRunFlowSequencer.updateFirstRunProperties(mFreProperties);

        BooleanSupplier showSearchEnginePromo =
                () -> mFreProperties.getBoolean(SHOW_SEARCH_ENGINE_PAGE);

        // An optional page to select a default search engine.
        if (showSearchEnginePromo.getAsBoolean()) {
            mPages.add(
                    new FirstRunPage<>(
                            DefaultSearchEngineFirstRunFragment.class, showSearchEnginePromo));
            mFreProgressStates.add(MobileFreProgress.DEFAULT_SEARCH_ENGINE_SHOWN);
        }

        // An optional history sync opt-in page, the visibility of this page will be decided on the
        // fly according to the situation.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            BooleanSupplier showHistorySync =
                    () -> mFreProperties.getBoolean(SHOW_HISTORY_SYNC_PAGE);
            if (!showHistorySync.getAsBoolean()) {
                HistorySyncHelper historySyncHelper =
                        HistorySyncHelper.getForProfile(
                                getProfileProviderSupplier().get().getOriginalProfile());
                historySyncHelper.recordHistorySyncNotShown(SigninAccessPoint.START_PAGE);
            }
            mPages.add(new FirstRunPage<>(HistorySyncFirstRunFragment.class, showHistorySync));
            mFreProgressStates.add(MobileFreProgress.HISTORY_SYNC_OPT_IN_SHOWN);
        } else {
            BooleanSupplier showSyncConsent =
                    () -> mFreProperties.getBoolean(SHOW_SYNC_CONSENT_PAGE);
            mPages.add(new FirstRunPage<>(SyncConsentFirstRunFragment.class, showSyncConsent));
            mFreProgressStates.add(MobileFreProgress.SYNC_CONSENT_SHOWN);
        }

        if (mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativeAndPolicyPagesCreated = true;

        if (sObserver != null) {
            sObserver.onCreatePostNativeAndPoliciesPageSequence(FirstRunActivity.this);
        }
    }

    @Override
    protected void onPreCreate() {
        // On tablets, where FRE activity is a dialog, transitions from fullscreen activities
        // (the ones that use Theme.Chromium.TabbedMode, e.g. ChromeTabbedActivity) look ugly,
        // because when FRE is started from CTA.onCreate(), currently running animation for CTA
        // window is aborted. This is perceived as a flash of white and doesn't look good.
        //
        // To solve this, we apply Theme.Chromium.TabbedMode on Tablet and Automotive here, to use
        // the same window background as other tabbed mode activities using the same theme.
        if (SigninUtils.isTabletOrAuto(this)) {
            setTheme(R.style.Theme_Chromium_TabbedMode);
        } else if (DialogWhenLargeContentLayout.shouldShowAsDialog(this)) {
            // For consistency with tablets, the status bar should be black on phones with large
            // screen, where the FRE is shown as dialog.
            StatusBarColorController.setStatusBarColor(getWindow(), Color.BLACK);
        }
        super.onPreCreate();
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
     * Creates the content view for this activity. The only thing subclasses can do is wrapping the
     * view returned by super implementation in some extra layout.
     */
    @CallSuper
    protected View createContentView() {
        mPager = new ViewPager2(this);

        // Disable swipe gesture.
        mPager.setUserInputEnabled(false);

        mPager.setId(R.id.fre_pager);
        mPager.setOffscreenPageLimit(3);
        return SigninUtils.wrapInDialogWhenLargeLayout(mPager);
    }

    @Override
    public void triggerLayoutInflation() {
        super.triggerLayoutInflation();

        initializeStateFromLaunchData();
        RecordHistogram.recordTimesHistogram(
                "MobileFre.FromLaunch.TriggerLayoutInflation",
                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

        setFinishOnTouchOutside(true);

        setContentView(createContentView());

        // SigninFirstRunFragment doesn't use getProperties() and can be shown right away, without
        // waiting for FirstRunFlowSequencer.
        createFirstPage();

        mFirstRunFlowSequencer =
                new FirstRunFlowSequencer(
                        getProfileProviderSupplier(), getChildAccountStatusSupplier()) {
                    @Override
                    public void onFlowIsKnown(Bundle freProperties) {
                        assert freProperties != null;
                        mFreProperties = freProperties;
                        RecordHistogram.recordTimesHistogram(
                                "MobileFre.FromLaunch.ChildStatusAvailable",
                                SystemClock.elapsedRealtime() - mIntentCreationElapsedRealtimeMs);

                        onInternalStateChanged();

                        recordFreProgressHistogram(mFreProgressStates.get(0));
                        long inflationCompletion = SystemClock.elapsedRealtime();
                        RecordHistogram.recordTimesHistogram(
                                "MobileFre.FromLaunch.FirstFragmentInflatedV2",
                                inflationCompletion - mIntentCreationElapsedRealtimeMs);
                        getFirstRunAppRestrictionInfo()
                                .getCompletionElapsedRealtimeMs(
                                        restrictionsCompletion -> {
                                            if (restrictionsCompletion > inflationCompletion) {
                                                RecordHistogram.recordTimesHistogram(
                                                        "MobileFre.FragmentInflationSpeed.FasterThanAppRestriction",
                                                        restrictionsCompletion
                                                                - inflationCompletion);
                                            } else {
                                                RecordHistogram.recordTimesHistogram(
                                                        "MobileFre.FragmentInflationSpeed.SlowerThanAppRestriction",
                                                        inflationCompletion
                                                                - restrictionsCompletion);
                                            }
                                        });
                    }
                };
        mFirstRunFlowSequencer.start();
        FirstRunStatus.setFirstRunTriggered(true);
        recordFreProgressHistogram(MobileFreProgress.STARTED);
        onInitialLayoutInflationComplete();

        RecordHistogram.recordTimesHistogram(
                "MobileFre.FromLaunch.ActivityInflated",
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

        Runnable onNativeFinished =
                () -> {
                    if (isActivityFinishingOrDestroyed()) return;

                    onNativeDependenciesFullyInitialized();
                };
        Profile profile = getProfileProviderSupplier().get().getOriginalProfile();
        TemplateUrlServiceFactory.getForProfile(profile).runWhenLoaded(onNativeFinished);
        // Notify feature engagement that FRE occurred.
        TrackerFactory.getTrackerForProfile(profile)
                .notifyEvent(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO);
        RecordHistogram.recordTimesHistogram(
                "MobileFre.NativeInitialized", SystemClock.elapsedRealtime() - getStartTime());
    }

    private void onNativeDependenciesFullyInitialized() {
        mNativeInitializationPromise.fulfill(null);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            mPager.setOffscreenPageLimit(ViewPager2.OFFSCREEN_PAGE_LIMIT_DEFAULT);
        }

        onInternalStateChanged();
    }

    @Override
    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {
        super.onPolicyLoadListenerAvailable(onDevicePolicyFound);
        RecordHistogram.recordTimesHistogram(
                "MobileFre.FromLaunch.PoliciesLoaded",
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
        return mNativeInitializationPromise.isFulfilled()
                && isFlowKnown()
                && this.getPolicyLoadListener().get() != null;
    }

    /**
     * @param {boolean} smoothScroll Whether to animate transition. This should be true for user
     *     triggered transition, and false for quick skips by software.
     */
    private boolean advanceToNextPageInternal(boolean smoothScroll) {
        mFirstRunFlowSequencer.updateFirstRunProperties(mFreProperties);

        int position = mPager.getCurrentItem() + 1;
        while (position < mPagerAdapter.getItemCount() && !mPages.get(position).shouldShow()) {
            ++position;
        }
        if (!setCurrentItemForPager(position, smoothScroll)) return false;

        recordFreProgressHistogram(mFreProgressStates.get(position));
        return true;
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
            mNativeInitializationPromise.then(
                    (ignored) -> {
                        page.onNativeInitialized();
                    });
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
            setCurrentItemForPager(position, true);
        }
        return BackPressResult.SUCCESS;
    }

    @Override
    public int getSecondaryActivity() {
        return SecondaryActivity.FIRST_RUN;
    }

    // FirstRunPageDelegate:
    @Override
    public Bundle getProperties() {
        return mFreProperties;
    }

    @Override
    public boolean advanceToNextPage() {
        return advanceToNextPageInternal(true);
    }

    @Override
    public void abortFirstRunExperience() {
        finish();

        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
        if (sObserver != null) sObserver.onAbortFirstRunExperience(this);
    }

    @Override
    public void completeFirstRunExperience() {
        RecordHistogram.recordMediumTimesHistogram(
                "MobileFre.FromLaunch.FreCompleted",
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
            ApplicationStatus.registerStateListenerForAllActivities(
                    new ActivityStateListener() {
                        @Override
                        public void onActivityStateChange(Activity activity, int newState) {
                            boolean shouldFinish = false;
                            if (activity == FirstRunActivity.this) {
                                shouldFinish =
                                        (newState == ActivityState.STOPPED
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
        return mLaunchedFromCct;
    }

    @Override
    public void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        assert mNativeInitializationPromise.isFulfilled();

        // If default is true then it corresponds to opt-out and false corresponds to opt-in.
        UmaUtils.recordMetricsReportingDefaultOptIn(!DEFAULT_METRICS_AND_CRASH_REPORTING);
        RecordHistogram.recordMediumTimesHistogram(
                "MobileFre.FromLaunch.TosAccepted",
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
            mLaunchedFromCct =
                    getIntent().getBooleanExtra(EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
            mIntentCreationElapsedRealtimeMs =
                    getIntent().getLongExtra(EXTRA_FRE_INTENT_CREATION_ELAPSED_REALTIME_MS, 0);
        }
    }

    private boolean isRtl() {
        return LocalizationUtils.isLayoutRtl();
    }

    private boolean setCurrentItemForPager(int position, boolean smoothScroll) {
        if (sObserver != null) sObserver.onJumpToPage(this, position);

        if (position >= mPagerAdapter.getItemCount()) {
            completeFirstRunExperience();
            return false;
        }

        int oldPosition = mPager.getCurrentItem();

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

        if (!sIsAnimationDisabled && smoothScroll) {
            // Use fake drags to control transition time and interpolation in ViewPager2.
            int width = getWindow().getDecorView().getWidth();
            // Direction of content shift: Forward -> negative; backward -> positive (assuming LTR).
            int direction = (position > oldPosition) ? -1 : 1;
            mPageTransformer.setDirection(direction);
            // Direction of drag, which is flipped if RTL.
            float dragSign = direction * (isRtl() ? -1f : 1f);
            // Use linear interpolation to enable custom interpolators usage of various properties.
            ValueAnimator animator = ValueAnimator.ofInt(0, width);
            animator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

            class UpdateListener implements ValueAnimator.AnimatorUpdateListener {
                /** Previous animated value to calculate fake drag delta for transitions. */
                private int mPrevAnimatedValue; // Initializes to 0, as desired.

                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    int animatedValue = ((Integer) animation.getAnimatedValue()).intValue();
                    float deltaPx = dragSign * (animatedValue - mPrevAnimatedValue);
                    mPager.fakeDragBy(deltaPx);
                    mPrevAnimatedValue = animatedValue;
                }
            }

            animator.addUpdateListener(new UpdateListener());

            animator.addListener(
                    new ValueAnimator.AnimatorListener() {
                        @Override
                        public void onAnimationStart(Animator animation) {
                            mPager.beginFakeDrag();
                        }

                        @Override
                        public void onAnimationEnd(Animator animation) {
                            mPager.endFakeDrag();
                            // No need to call `mPager.setCurrentItem(position, false)`.
                        }

                        @Override
                        public void onAnimationCancel(Animator animation) {
                            /* Ignored */
                        }

                        @Override
                        public void onAnimationRepeat(Animator animation) {
                            /* Ignored */
                        }
                    });

            animator.setDuration(TRANSITION_DELAY_MS);
            animator.start();

        } else {
            mPager.setCurrentItem(position, false);
        }

        return true;
    }

    private void skipPagesIfNecessary() {
        while (!mPages.get(mPager.getCurrentItem()).shouldShow()
                && advanceToNextPageInternal(false)) {}
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
        RecordHistogram.recordTimesHistogram(
                "MobileFre.FromLaunch.NativeInitialized",
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

    public FirstRunFragment getCurrentFragmentForTesting() {
        return mPagerAdapter.getFirstRunFragment(mPager.getCurrentItem());
    }

    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }

    public static void disableAnimationForTesting(boolean isAnimationDisabled) {
        sIsAnimationDisabled = isAnimationDisabled;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }
}
