// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.graphics.drawable.Animatable2;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 *  A controller responsible for setting up quick delete.
 */
public class QuickDeleteController implements QuickDeleteBridge.DomainVisitsCallback {
    private static final MutableFlagWithSafeDefault sQuickDeleteForAndroidFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID, false);

    private final @NonNull Context mContext;
    private final @NonNull QuickDeleteDelegate mDelegate;
    private final @NonNull QuickDeleteDialogDelegate mDialogDelegate;
    private final @NonNull QuickDeleteTabsFilter mDeleteTabsFilter;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull LayoutManager mLayoutManager;
    private final @NonNull View mAnimationView;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context The associated {@link Context}.
     * @param delegate A {@link QuickDeleteDelegate} to perform the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     * @param layoutManager {@link LayoutManager} to use for showing the regular overview mode.
     * @param tabModelSelector {@link TabModelSelector} to use for opening the links in search
     *         history disambiguation notice.
     * @param animationView The {@link View} to use to show the quick delete animation.
     */
    public QuickDeleteController(@NonNull Context context, @NonNull QuickDeleteDelegate delegate,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager, @NonNull LayoutManager layoutManager,
            @NonNull TabModelSelector tabModelSelector, @NonNull View animationView) {
        mContext = context;
        mDelegate = delegate;
        mSnackbarManager = snackbarManager;
        mLayoutManager = layoutManager;
        mDeleteTabsFilter =
                new QuickDeleteTabsFilter(tabModelSelector.getModel(/*incognito=*/false));
        Profile profile = tabModelSelector.getCurrentModel().getProfile();
        mDialogDelegate = new QuickDeleteDialogDelegate(
                context, modalDialogManager, this::onDialogDismissed, tabModelSelector, profile);

        mAnimationView = animationView;
        mAnimationView.setBackgroundResource(R.drawable.quick_delete_animation);

        new QuickDeleteBridge(profile).getLastVisitedDomainAndUniqueDomainCount(this);
    }

    /**
     * @return True, if quick delete feature flag is enabled, false otherwise.
     */
    public static boolean isQuickDeleteEnabled() {
        return sQuickDeleteForAndroidFlag.isEnabled();
    }

    /**
     * A method called when the user confirms or cancels the dialog.
     */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);
                @TimePeriod
                int timePeriod = mDialogDelegate.getCurrentTimePeriodOption().getTimePeriod();
                // TODO(crbug.com/1412087): This is hardcoded to work on 15 minutes time range.
                // Update this to take into account |timePeriod| and also deletion of any remaining
                // tabs inside |mDeleteTabsFilter|. For example, when the quick delete dialog was
                // shown and the user didn't interact with they come back to do an action after some
                // time, then calling QuickDeleteTabFilter#closeTabsFilteredForQuickDelete method
                // would only delete tabs in the last 15 minutes and may miss the tabs in
                // |mDeleteTabsFilter|.
                mDeleteTabsFilter.closeTabsFilteredForQuickDelete();
                // TODO(crbug.com/1412087): This may have similar considerations as above and
                // therefore may need to be revisited based on what Clear browsing data is doing
                // today.
                mDelegate.performQuickDelete(this::onQuickDeleteFinished, timePeriod);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);
                break;
            default:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);
                break;
        }
    }

    private void onQuickDeleteFinished() {
        navigateToTabSwitcher();
        triggerHapticFeedback();
        showDeleteAnimation(this::showSnackbar);
    }

    /**
     * A method to navigate to tab switcher.
     */
    private void navigateToTabSwitcher() {
        if (mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) return;
        mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, /*animate=*/true);
    }

    private void triggerHapticFeedback() {
        Vibrator v = (Vibrator) mContext.getSystemService(Context.VIBRATOR_SERVICE);
        final long duration = 50;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            v.vibrate(VibrationEffect.createOneShot(duration, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            // Deprecated in API 26.
            v.vibrate(duration);
        }
    }

    /**
     * A method to show the quick delete snack-bar.
     */
    private void showSnackbar() {
        Snackbar snackbar = Snackbar.make(
                mContext.getString(R.string.quick_delete_snackbar_message),
                /*controller= */ null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_QUICK_DELETE);

        mSnackbarManager.showSnackbar(snackbar);
    }

    private void showDeleteAnimation(@NonNull Runnable onAnimationEnd) {
        AnimatedVectorDrawable deleteAnimation =
                (AnimatedVectorDrawable) mAnimationView.getBackground();
        mAnimationView.setVisibility(View.VISIBLE);
        deleteAnimation.registerAnimationCallback(new Animatable2.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                super.onAnimationEnd(drawable);
                ((AnimatedVectorDrawable) drawable).unregisterAnimationCallback(this);
                mAnimationView.setVisibility(View.GONE);
                onAnimationEnd.run();
            }
        });
        deleteAnimation.start();
    }

    /**
     * Called when the domain count and last visited domain are fetched from local history.
     * This method passes the data needed for constructing the dialog and shows the quick
     * delete dialog.
     * @param lastVisitedDomain The synced last visited domain on all devices in the last 15
     *                          minutes.
     * @param domainCount The number of synced unique domains visited on all devices in the
     *                    last 15 minutes.
     */
    @Override
    public void onLastVisitedDomainAndUniqueDomainCountReady(
            String lastVisitedDomain, int domainCount) {
        QuickDeleteDialogDelegate.QuickDeleteDialogData data =
                new QuickDeleteDialogDelegate.QuickDeleteDialogData(lastVisitedDomain, domainCount,
                        mDeleteTabsFilter.getListOfTabsToBeClosed().size());
        mDialogDelegate.showDialog(data);
    }
}
