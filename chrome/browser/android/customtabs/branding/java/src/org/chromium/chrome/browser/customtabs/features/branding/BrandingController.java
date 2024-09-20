// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.content.Context;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.components.crash.PureJavaExceptionReporter;
import org.chromium.ui.widget.Toast;

import java.util.concurrent.TimeUnit;

/** Controls the strategy to start branding, and the duration to show branding. */
public class BrandingController {
    private static final String TAG = "CctBrand";

    /**
     * The maximum time allowed from CCT Toolbar initialized until it should show the URL and title.
     */
    @VisibleForTesting static final int TOTAL_BRANDING_DELAY_MS = 1800;

    /**
     * The maximum time allowed to leave CCT Toolbar blank until showing branding or URL and title.
     */
    @VisibleForTesting static final int MAX_BLANK_TOOLBAR_TIMEOUT_MS = 500;

    /**
     * The minimum time required between two branding events to shown. If time elapsed since last
     * branding is less than this cadence, the branding check decision will be {@link
     * BrandingDecision.NONE}.
     */
    @VisibleForTesting static final int BRANDING_CADENCE_MS = (int) TimeUnit.HOURS.toMillis(1);

    private final CallbackController mCallbackController = new CallbackController();
    private final @BrandingDecision OneshotSupplierImpl<Integer> mBrandingDecision =
            new OneshotSupplierImpl<>();
    private final BrandingChecker mBrandingChecker;
    private final Context mContext;
    private final String mBrowserName;
    private final int mToastTemplateId;
    @Nullable private final PureJavaExceptionReporter mExceptionReporter;
    private ToolbarBrandingDelegate mToolbarBrandingDelegate;
    private @Nullable Toast mToast;
    private long mToolbarInitializedTime;
    private boolean mIsDestroyed;

    /**
     * Branding controller responsible for showing branding.
     *
     * @param context Context used to fetch package information for embedded app.
     * @param appId The ID for the embedded app. Can be {@code null}
     * @param browserName The browser name shown on the branding toast.
     * @param toastTemplateId Resource ID of the string to be shown on Toast branding UI.
     * @param exceptionReporter Optional reporter that reports wrong state quietly.
     */
    public BrandingController(
            Context context,
            String appId,
            String browserName,
            @StringRes int toastTemplateId,
            @Nullable PureJavaExceptionReporter exceptionReporter) {
        mContext = context;
        mBrowserName = browserName;
        mToastTemplateId = toastTemplateId;
        mExceptionReporter = exceptionReporter;
        mBrandingDecision.onAvailable(
                mCallbackController.makeCancelable((decision) -> maybeMakeBrandingDecision()));

        // TODO(crbug.com/40234239): Start branding checker during CCT warm up.
        mBrandingChecker =
                new BrandingChecker(
                        appId,
                        SharedPreferencesBrandingTimeStorage.getInstance(),
                        mBrandingDecision::set,
                        BRANDING_CADENCE_MS,
                        BrandingDecision.TOAST);
        mBrandingChecker.executeWithTaskTraits(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    }

    /**
     * Register the {@link ToolbarBrandingDelegate} from CCT Toolbar.
     * @param delegate {@link ToolbarBrandingDelegate} instance from CCT Toolbar.
     */
    public void onToolbarInitialized(@NonNull ToolbarBrandingDelegate delegate) {
        if (mIsDestroyed) {
            reportErrorMessage("BrandingController should not be access after destroyed.");
            return;
        }

        mToolbarInitializedTime = SystemClock.elapsedRealtime();
        mToolbarBrandingDelegate = delegate;

        // Start the task to timeout the branding check. If mBrandingChecker already finished,
        // canceling the task does nothing. Does not interrupt if the task is running, since the
        // BrandingChecker#doInBackground will collect metrics at the end.
        PostTask.postDelayedTask(
                TaskTraits.UI_USER_VISIBLE,
                mCallbackController.makeCancelable(
                        () -> mBrandingChecker.cancel(/* mayInterruptIfRunning= */ false)),
                MAX_BLANK_TOOLBAR_TIMEOUT_MS);

        // Set location bar to empty as controller is waiting for mBrandingDecision.
        // This should not cause any UI jank even if a decision is made immediately, as
        // state set in CustomTabToolbar#showEmptyLocationBar should be unset in any newer state.
        mToolbarBrandingDelegate.showEmptyLocationBar();

        maybeMakeBrandingDecision();
    }

    /** Make decision after BrandingChecker and mToolbarBrandingDelegate is ready. */
    private void maybeMakeBrandingDecision() {
        if (mToolbarBrandingDelegate == null || mBrandingDecision.get() == null) return;

        long timeToolbarEmpty = SystemClock.elapsedRealtime() - mToolbarInitializedTime;
        long remainingBrandingTime = TOTAL_BRANDING_DELAY_MS - timeToolbarEmpty;

        @BrandingDecision int brandingDecision = mBrandingDecision.get();
        switch (brandingDecision) {
            case BrandingDecision.NONE:
                mToolbarBrandingDelegate.showRegularToolbar();
                break;
            case BrandingDecision.TOOLBAR:
                showToolbarBranding(remainingBrandingTime);
                break;
            case BrandingDecision.TOAST:
                mToolbarBrandingDelegate.showRegularToolbar();
                showToastBranding(remainingBrandingTime);
                break;
            default:
                assert false : "Unreachable state!";
        }

        finish();
    }

    private void showToolbarBranding(long durationMs) {
        mToolbarBrandingDelegate.showBrandingLocationBar();

        Runnable hideToolbarBranding =
                () -> {
                    mToolbarBrandingDelegate.showRegularToolbar();
                };
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                mCallbackController.makeCancelable(hideToolbarBranding),
                durationMs);
    }

    private void showToastBranding(long durationMs) {
        if (mIsDestroyed) {
            reportErrorMessage("Toast should not get accessed after destroyed.");
            return;
        }

        String toastText = mContext.getResources().getString(mToastTemplateId, mBrowserName);
        TextView runInChromeTextView =
                (TextView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.custom_tabs_toast_branding_layout, null, false);
        runInChromeTextView.setText(toastText);

        Toast toast =
                new Toast(mContext.getApplicationContext(), /* toastView= */ runInChromeTextView);
        toast.setDuration(Toast.LENGTH_LONG);
        toast.show();
        PostTask.postDelayedTask(
                TaskTraits.UI_BEST_EFFORT,
                mCallbackController.makeCancelable(toast::cancel),
                durationMs);
    }

    /** Prevent any updates to this instance and cancel all scheduled callbacks. */
    public void destroy() {
        mIsDestroyed = true;
        mCallbackController.destroy();
        mBrandingChecker.cancel(true);
        if (mToast != null) {
            mToast.cancel();
        }
    }

    private void reportErrorMessage(String message) {
        Log.e(TAG, message);
        if (mExceptionReporter != null) {
            mExceptionReporter.createAndUploadReport(new Throwable(message));
        }
    }

    private void finish() {
        // Post the task as it's not important to be complete during branding check.
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                mCallbackController.makeCancelable(
                        () -> {
                            int numberOfPackages =
                                    SharedPreferencesBrandingTimeStorage.getInstance().getSize();
                            RecordHistogram.recordCount100Histogram(
                                    "CustomTabs.Branding.NumberOfClients", numberOfPackages);

                            // Release the in-memory share pref from the current session if branding
                            // checker didn't timeout.
                            if (!mBrandingChecker.isCancelled()) {
                                SharedPreferencesBrandingTimeStorage.resetInstance();
                            }
                        }));
    }

    @BrandingDecision
    Integer getBrandingDecisionForTest() {
        return mBrandingDecision.get();
    }
}
