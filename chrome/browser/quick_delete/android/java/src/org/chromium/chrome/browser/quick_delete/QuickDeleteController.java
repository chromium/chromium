// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.Tracker.DisplayLockHandle;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** A controller responsible for setting up quick delete MVC. */
@NullMarked
public class QuickDeleteController {
    // LINT.IfChange(TipsPrefNames)
    public static final String QUICK_DELETE_EVER_USED_PREF = "browser.quick_delete_ever_used";
    // LINT.ThenChange(//components/browsing_data/core/pref_names.h:TipsPrefNames)

    private final Context mContext;
    private final QuickDeleteDelegate mDelegate;
    private final QuickDeleteTabsFilter mDeleteRegularTabsFilter;
    // Null when declutter is disabled.
    private final @Nullable QuickDeleteTabsFilter mDeleteArchivedTabsFilter;
    private final TabModelSelector mTabModelSelector;
    private final SnackbarManager mSnackbarManager;
    private final @Nullable LayoutManager mLayoutManager;
    private final Profile mProfile;
    private final TabModel mTabModel;
    private final QuickDeleteMediator mQuickDeleteMediator;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final QuickDeleteDialogDelegate mDialogDelegate;
    private final CallbackController mCallbackController = new CallbackController();
    private final TabModelSelectorObserver mRegularTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onDestroyed() {
                    destroy();
                }
            };
    private boolean mIsDestroyed;
    private @Nullable DisplayLockHandle mTrackerLock;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context The associated {@link Context}.
     * @param delegate A {@link QuickDeleteDelegate} to perform the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     * @param layoutManager {@link LayoutManager} to use for showing the regular overview mode.
     * @param tabModelSelector {@link TabModelSelector} for regular tabs.
     * @param archivedTabModelSelector The {@link TabModelSelector} for archived tabs.
     */
    public QuickDeleteController(
            Context context,
            QuickDeleteDelegate delegate,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            @Nullable LayoutManager layoutManager,
            TabModelSelector tabModelSelector,
            @Nullable TabModelSelector archivedTabModelSelector) {
        mContext = context;
        mDelegate = delegate;
        mSnackbarManager = snackbarManager;
        mLayoutManager = layoutManager;
        mTabModelSelector = tabModelSelector;

        mTabModel = tabModelSelector.getModel(/* incognito= */ false);
        mDeleteRegularTabsFilter = new QuickDeleteTabsFilter(mTabModel);
        if (archivedTabModelSelector != null) {
            mDeleteArchivedTabsFilter =
                    new QuickDeleteTabsFilter(
                            archivedTabModelSelector.getModel(/* incognito= */ false));
        } else {
            mDeleteArchivedTabsFilter = null;
        }
        mProfile = assumeNonNull(mTabModel.getProfile());

        // MVC setup.
        View quickDeleteView =
                LayoutInflater.from(context).inflate(R.layout.quick_delete_dialog, null);
        mPropertyModel =
                new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS)
                        .with(QuickDeleteProperties.CONTEXT, mContext)
                        .with(
                                QuickDeleteProperties.HAS_MULTI_WINDOWS,
                                delegate.isInMultiWindowMode())
                        .with(
                                QuickDeleteProperties.IS_HISTORY_DELETION_ALLOWED,
                                UserPrefs.get(mProfile).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY))
                        .build();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, quickDeleteView, QuickDeleteViewBinder::bind);
        mQuickDeleteMediator =
                new QuickDeleteMediator(
                        mPropertyModel,
                        mProfile,
                        mDeleteRegularTabsFilter,
                        mDeleteArchivedTabsFilter);

        mDialogDelegate =
                new QuickDeleteDialogDelegate(
                        context,
                        quickDeleteView,
                        modalDialogManager,
                        this::onDialogDismissed,
                        tabModelSelector,
                        mQuickDeleteMediator);

        mTabModelSelector.addObserver(mRegularTabModelSelectorObserver);
    }

    /** Destroys the controller and cancels any in-flight deletion callbacks. */
    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;
        mCallbackController.destroy();
        mTabModelSelector.removeObserver(mRegularTabModelSelectorObserver);
        mDialogDelegate.dismissDialog(DialogDismissalCause.ACTIVITY_DESTROYED);
        mPropertyModelChangeProcessor.destroy();
        mQuickDeleteMediator.destroy();
        if (mTrackerLock != null) {
            mTrackerLock.release();
            mTrackerLock = null;
        }
    }

    /** Show the Quick Delete dialog. */
    public void showDialog() {
        if (mIsDestroyed) return;
        mDialogDelegate.showDialog();
    }

    /** A method called when the user confirms or cancels the dialog. */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        if (mIsDestroyed) return;
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);
                @TimePeriod int timePeriod = mPropertyModel.get(QuickDeleteProperties.TIME_PERIOD);
                boolean isTabClosureDisabled =
                        mPropertyModel.get(QuickDeleteProperties.HAS_MULTI_WINDOWS);

                mDelegate.performQuickDelete(
                        mCallbackController.makeCancelable(
                                () ->
                                        onBrowsingDataDeletionFinished(
                                                timePeriod, isTabClosureDisabled)),
                        timePeriod);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);
                destroy();
                break;
            default:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);
                destroy();
                break;
        }
    }

    private void onBrowsingDataDeletionFinished(
            @TimePeriod int timePeriod, boolean isTabClosureDisabled) {
        RecordHistogram.recordBooleanHistogram(
                "Privacy.QuickDelete.TabsEnabled", !isTabClosureDisabled);
        UserPrefs.get(mProfile).setBoolean(QUICK_DELETE_EVER_USED_PREF, true);

        // Ensure that no in-product help is triggered during tab closure and the post-deletion
        // experience.
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        mTrackerLock = tracker.acquireDisplayLock();

        if (isTabClosureDisabled) {
            showPostDeleteFeedback(timePeriod);
        } else {
            if (mLayoutManager != null) {
                TabSwitcherUtils.navigateToTabSwitcher(
                        mLayoutManager,
                        /* animate= */ true,
                        mCallbackController.makeCancelable(
                                () -> maybeShowQuickDeleteAnimation(timePeriod)));
            } else {
                maybeShowQuickDeleteAnimation(timePeriod);
            }
        }
    }

    private void maybeShowQuickDeleteAnimation(@TimePeriod int timePeriod) {
        mDeleteRegularTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        if (mDeleteArchivedTabsFilter != null) {
            mDeleteArchivedTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        }
        boolean isTabModelEmpty = mTabModel.getCount() == 0;
        // If the tab switcher is not displayed, skip the animation.
        boolean isTabSwitcherVisible =
                mLayoutManager != null && mLayoutManager.isLayoutVisible(LayoutType.HUB);

        if (!isTabModelEmpty && isTabSwitcherVisible) {
            List<Tab> tabs =
                    mDeleteRegularTabsFilter
                            .getListOfTabsFilteredToBeClosedExcludingPlaceholderTabGroups();
            mDelegate.showQuickDeleteAnimation(
                    mCallbackController.makeCancelable(
                            () -> closeTabsAndShowPostDeleteFeedback(timePeriod)),
                    tabs);
        } else {
            closeTabsAndShowPostDeleteFeedback(timePeriod);
        }
    }

    private void closeTabsAndShowPostDeleteFeedback(@TimePeriod int timePeriod) {
        mDeleteRegularTabsFilter.closeTabsFilteredForQuickDelete();
        if (mDeleteArchivedTabsFilter != null) {
            mDeleteArchivedTabsFilter.closeTabsFilteredForQuickDelete();
        }
        showPostDeleteFeedback(timePeriod);
    }

    private void showPostDeleteFeedback(@TimePeriod int timePeriod) {
        triggerHapticFeedback();
        showSnackbar(timePeriod);

        destroy();
    }

    private void triggerHapticFeedback() {
        Vibrator v = (Vibrator) mContext.getSystemService(Context.VIBRATOR_SERVICE);
        final long duration = 50;
        v.vibrate(VibrationEffect.createOneShot(duration, VibrationEffect.DEFAULT_AMPLITUDE));
    }

    /** A method to show the quick delete snack-bar. */
    private void showSnackbar(@TimePeriod int timePeriod) {
        String snackbarMessage;
        if (timePeriod == TimePeriod.ALL_TIME) {
            snackbarMessage = mContext.getString(R.string.quick_delete_snackbar_all_time_message);
        } else {
            snackbarMessage =
                    mContext.getString(
                            R.string.quick_delete_snackbar_message,
                            TimePeriodUtils.getTimePeriodString(mContext, timePeriod));
        }
        Snackbar snackbar =
                Snackbar.make(
                        snackbarMessage,
                        /* controller= */ null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_QUICK_DELETE);
        mSnackbarManager.showSnackbar(snackbar);
    }
}