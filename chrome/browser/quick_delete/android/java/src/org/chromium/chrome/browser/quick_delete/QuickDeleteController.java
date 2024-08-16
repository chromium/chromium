// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** A controller responsible for setting up quick delete MVC. */
public class QuickDeleteController {

    private final @NonNull Context mContext;
    private final @NonNull QuickDeleteDelegate mDelegate;
    private final @NonNull QuickDeleteTabsFilter mDeleteRegularTabsFilter;
    // Null when declutter is disabled.
    private final @Nullable QuickDeleteTabsFilter mDeleteArchivedTabsFilter;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull LayoutManager mLayoutManager;
    private final @NonNull Profile mProfile;
    private final @NonNull TabModel mTabModel;
    private final QuickDeleteBridge mQuickDeleteBridge;
    private final QuickDeleteMediator mQuickDeleteMediator;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final TabModelSelector mTabModelSelector;
    private CurrentTabObserver mCurrentTabObserver;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context The associated {@link Context}.
     * @param delegate A {@link QuickDeleteDelegate} to perform the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     * @param layoutManager {@link LayoutManager} to use for showing the regular overview mode.
     * @param tabModelSelector {@link TabModelSelector} to use for opening the links in search
     *     history disambiguation notice.
     * @param archivedTabModel The {@link TabModel} for archived tabs.
     */
    public QuickDeleteController(
            @NonNull Context context,
            @NonNull QuickDeleteDelegate delegate,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull LayoutManager layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @Nullable TabModelSelector archivedTabModelSelector) {
        mContext = context;
        mDelegate = delegate;
        mSnackbarManager = snackbarManager;
        mLayoutManager = layoutManager;

        mTabModel = tabModelSelector.getModel(/* incognito= */ false);
        mDeleteRegularTabsFilter =
                new QuickDeleteTabsFilter(
                        (TabGroupModelFilter)
                                tabModelSelector
                                        .getTabModelFilterProvider()
                                        .getTabModelFilter(/* incognito= */ false));
        if (archivedTabModelSelector != null) {
            mDeleteArchivedTabsFilter =
                    new QuickDeleteTabsFilter(
                            (TabGroupModelFilter)
                                    archivedTabModelSelector
                                            .getTabModelFilterProvider()
                                            .getTabModelFilter(/* incognito= */ false));
        } else {
            mDeleteArchivedTabsFilter = null;
        }
        mProfile = tabModelSelector.getCurrentModel().getProfile();
        mQuickDeleteBridge = new QuickDeleteBridge(mProfile);
        mTabModelSelector = tabModelSelector;

        // MVC setup.
        View quickDeleteView =
                LayoutInflater.from(context).inflate(R.layout.quick_delete_dialog, null);
        mPropertyModel =
                new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS)
                        .with(QuickDeleteProperties.CONTEXT, mContext)
                        .with(
                                QuickDeleteProperties.HAS_MULTI_WINDOWS,
                                delegate.isInMultiWindowMode())
                        .build();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, quickDeleteView, QuickDeleteViewBinder::bind);
        mQuickDeleteMediator =
                new QuickDeleteMediator(
                        mPropertyModel,
                        mProfile,
                        mQuickDeleteBridge,
                        mDeleteRegularTabsFilter,
                        mDeleteArchivedTabsFilter);

        QuickDeleteDialogDelegate dialogDelegate =
                new QuickDeleteDialogDelegate(
                        context,
                        quickDeleteView,
                        modalDialogManager,
                        this::onDialogDismissed,
                        tabModelSelector,
                        mQuickDeleteMediator);
        dialogDelegate.showDialog();
    }

    void destroy() {
        mPropertyModelChangeProcessor.destroy();
        mQuickDeleteBridge.destroy();

        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
        }
    }

    /**
     * @return True, if quick delete feature flag is enabled, false otherwise
     */
    public static boolean isQuickDeleteEnabled() {
        return ChromeFeatureList.sQuickDeleteForAndroid.isEnabled();
    }

    /**
     * @return True, if quick delete follow up is enabled, false otherwise
     */
    public static boolean isQuickDeleteFollowupEnabled() {
        return isQuickDeleteEnabled() && ChromeFeatureList.sQuickDeleteAndroidFollowup.isEnabled();
    }

    /**
     * @return True, if quick delete survey is enabled, false otherwise
     */
    public static boolean isQuickDeleteSurveyEnabled() {
        return isQuickDeleteFollowupEnabled()
                && ChromeFeatureList.sQuickDeleteAndroidSurvey.isEnabled();
    }

    /** A method called when the user confirms or cancels the dialog. */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);
                @TimePeriod int timePeriod = mPropertyModel.get(QuickDeleteProperties.TIME_PERIOD);
                boolean isTabClosureDisabled =
                        mPropertyModel.get(QuickDeleteProperties.HAS_MULTI_WINDOWS);

                mDelegate.performQuickDelete(
                        () -> onBrowsingDataDeletionFinished(timePeriod, isTabClosureDisabled),
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

        // Ensure that no in-product help is triggered during tab closure and the post-deletion
        // experience.
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        Tracker.DisplayLockHandle trackerLock = tracker.acquireDisplayLock();

        if (isTabClosureDisabled) {
            showPostDeleteFeedback(timePeriod, trackerLock);
        } else {
            TabSwitcherUtils.navigateToTabSwitcher(
                    mLayoutManager,
                    /* animate= */ true,
                    () -> maybeShowQuickDeleteAnimation(timePeriod, trackerLock));
        }
    }

    private void maybeShowQuickDeleteAnimation(
            @TimePeriod int timePeriod, @Nullable Tracker.DisplayLockHandle trackerLock) {
        mDeleteRegularTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        if (mDeleteArchivedTabsFilter != null) {
            mDeleteArchivedTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        }
        boolean isTabModelEmpty = mTabModel.getCount() == 0;

        if (isQuickDeleteFollowupEnabled() && !isTabModelEmpty) {
            List<Tab> tabs = mDeleteRegularTabsFilter.getListOfTabsFilteredToBeClosed();
            mDelegate.showQuickDeleteAnimation(
                    () -> closeTabsAndShowPostDeleteFeedback(timePeriod, trackerLock), tabs);
        } else {
            closeTabsAndShowPostDeleteFeedback(timePeriod, trackerLock);
        }
    }

    private void closeTabsAndShowPostDeleteFeedback(
            @TimePeriod int timePeriod, @Nullable Tracker.DisplayLockHandle trackerLock) {
        mDeleteRegularTabsFilter.closeTabsFilteredForQuickDelete();
        if (mDeleteArchivedTabsFilter != null) {
            mDeleteArchivedTabsFilter.closeTabsFilteredForQuickDelete();
        }
        showPostDeleteFeedback(timePeriod, trackerLock);
    }

    private void showPostDeleteFeedback(
            @TimePeriod int timePeriod, @Nullable Tracker.DisplayLockHandle trackerLock) {
        triggerHapticFeedback();
        showSnackbar(timePeriod);

        if (trackerLock == null) return;
        trackerLock.release();

        if (isQuickDeleteSurveyEnabled()) {
            assert mCurrentTabObserver == null;
            mCurrentTabObserver =
                    new CurrentTabObserver(
                            mTabModelSelector.getCurrentTabSupplier(),
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                                    WebContents webContents = tab.getWebContents();
                                    if (!tab.isOffTheRecord() && webContents != null) {
                                        showSurvey(webContents);
                                    }
                                }
                            },
                            /* swapCallback= */ null);
        }
    }

    /**
     * @see {@link QuickDeleteBridge#showSurvey(WebContents)}
     */
    private void showSurvey(@NonNull WebContents webContents) {
        mQuickDeleteBridge.showSurvey(webContents);
        destroy();
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
