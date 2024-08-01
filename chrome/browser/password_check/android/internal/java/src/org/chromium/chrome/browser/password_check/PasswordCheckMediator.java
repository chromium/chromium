// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.HAS_MANUAL_CHANGE_BUTTON;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_CONFIRMATION_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_ORIGIN;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.LAUNCH_ACCOUNT_CHECKUP_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.RESTART_BUTTON_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.SHOW_CHECK_SUBTITLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_DIALOG_HANDLER;

import android.content.Context;
import android.content.DialogInterface;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckChangePasswordHelper;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckIconHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.ReauthReason;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

/**
 * Contains the logic for the PasswordCheck component. It sets the state of the model and reacts to
 * events like clicks.
 */
class PasswordCheckMediator
        implements PasswordCheckCoordinator.CredentialEventHandler, PasswordCheck.Observer {
    private static long sStatusUpdateDelayMillis = 1000;

    private final PasswordAccessReauthenticationHelper mReauthenticationHelper;
    private final PasswordCheckChangePasswordHelper mChangePasswordDelegate;
    private PropertyModel mModel;
    private PasswordCheckComponentUi.Delegate mDelegate;
    private Runnable mLaunchCheckupInAccount;
    private HashSet<CompromisedCredential> mPreCheckSet;
    private final PasswordCheckIconHelper mIconHelper;
    private long mLastStatusUpdate;
    private boolean mCctIsOpened;

    PasswordCheckMediator(
            PasswordCheckChangePasswordHelper changePasswordDelegate,
            PasswordAccessReauthenticationHelper reauthenticationHelper,
            PasswordCheckIconHelper passwordCheckIconHelper) {
        mChangePasswordDelegate = changePasswordDelegate;
        mReauthenticationHelper = reauthenticationHelper;
        mIconHelper = passwordCheckIconHelper;
    }

    void initialize(
            PropertyModel model,
            PasswordCheckComponentUi.Delegate delegate,
            @PasswordCheckReferrer int passwordCheckReferrer,
            Runnable launchCheckupInAccount) {
        mModel = model;
        mDelegate = delegate;
        mLaunchCheckupInAccount = launchCheckupInAccount;
        mCctIsOpened = false;

        PasswordCheckMetricsRecorder.recordPasswordCheckReferrer(passwordCheckReferrer);

        // If a run is scheduled to happen soon, initialize the UI as running to prevent flickering.
        // Otherwise, initialize the UI with last known state (defaults to IDLE before first run).
        boolean shouldRunCheck = passwordCheckReferrer != PasswordCheckReferrer.SAFETY_CHECK;
        onPasswordCheckStatusChanged(
                shouldRunCheck
                        ? PasswordCheckUIStatus.RUNNING
                        : getPasswordCheck().getCheckStatus());
        getPasswordCheck().addObserver(this, true);
        if (shouldRunCheck) {
            PasswordCheckMetricsRecorder.recordUiUserAction(
                    PasswordCheckUserAction.START_CHECK_AUTOMATICALLY);
            getPasswordCheck().startCheck();
        }
    }

    void onResumeFragment() {
        // If the fragment is resumed, a CCT is closed.
        mCctIsOpened = false;
    }

    void onUserLeavesCheckPage() {
        // A user can leave the page because they opened a CCT in browser. As a user is fixing a
        // compromised credential, don't count such a case as a user |DID_NOTHING| for the remaining
        // credentials.
        if (!mCctIsOpened) {
            // A user closes the check page.
            ListModel<ListItem> items = mModel.get(ITEMS);
            for (int i = 1; i < items.size(); i++) {
                PasswordCheckMetricsRecorder.recordCheckResolutionAction(
                        PasswordCheckResolutionAction.DID_NOTHING,
                        items.get(i).model.get(COMPROMISED_CREDENTIAL));
            }
        }
    }

    void destroy() {
        getPasswordCheck().removeObserver(this);
    }

    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        CompromisedCredential[] credentials = getPasswordCheck().getCompromisedCredentials();
        assert credentials != null;

        List<CompromisedCredential> credentialsList = Arrays.asList(credentials);
        sortCredentials(credentialsList);

        ListModel<ListItem> items = mModel.get(ITEMS);
        if (items.size() == 0) {
            items.add(
                    new ListItem(
                            PasswordCheckProperties.ItemType.HEADER,
                            new PropertyModel.Builder(
                                            PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                                    .with(CHECK_STATUS, PasswordCheckUIStatus.RUNNING)
                                    .with(LAUNCH_ACCOUNT_CHECKUP_ACTION, mLaunchCheckupInAccount)
                                    .with(RESTART_BUTTON_ACTION, this::startCheckManually)
                                    .build()));
            mLastStatusUpdate = System.currentTimeMillis();
        }
        if (items.size() > 1) items.removeRange(1, items.size() - 1);

        updateStatusHeaderWhenCredentialsChange();
        for (CompromisedCredential credential : credentialsList) {
            items.add(createEntryForCredential(credential));
        }
    }

    @Override
    public void onSavedPasswordsFetchCompleted() {}

    @Override
    public void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status) {
        long currentTime = System.currentTimeMillis();

        if (shouldDelayStatusChange(status, currentTime)) {
            mLastStatusUpdate += sStatusUpdateDelayMillis;
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> changePasswordCheckStatus(status),
                    mLastStatusUpdate - currentTime);
        } else {
            mLastStatusUpdate = currentTime;
            changePasswordCheckStatus(status);
        }
    }

    private void changePasswordCheckStatus(@PasswordCheckUIStatus int status) {
        // There is no UI representation of a canceled check. This status can be sent when
        // the bridge and the password check UI are being torn down while a check is running.
        if (status == PasswordCheckUIStatus.CANCELED) return;
        ListModel<ListItem> items = mModel.get(ITEMS);
        PropertyModel header;
        if (items.size() == 0) {
            header =
                    new PropertyModel.Builder(PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                            .with(CHECK_PROGRESS, UNKNOWN_PROGRESS)
                            .with(CHECK_STATUS, PasswordCheckUIStatus.RUNNING)
                            .with(CHECK_TIMESTAMP, null)
                            .with(COMPROMISED_CREDENTIALS_COUNT, null)
                            .with(LAUNCH_ACCOUNT_CHECKUP_ACTION, mLaunchCheckupInAccount)
                            .with(RESTART_BUTTON_ACTION, this::startCheckManually)
                            .with(SHOW_CHECK_SUBTITLE, false)
                            .build();
        } else {
            header = items.get(0).model;
        }
        @PasswordCheckUIStatus int oldStatus = header.get(CHECK_STATUS);
        header.set(CHECK_STATUS, status);
        Pair<Integer, Integer> progress = header.get(CHECK_PROGRESS);
        if (progress == null) progress = UNKNOWN_PROGRESS;
        header.set(CHECK_PROGRESS, status == PasswordCheckUIStatus.RUNNING ? progress : null);
        Long checkTimestamp = null;
        Integer compromisedCredentialCount = null;
        if (status == PasswordCheckUIStatus.IDLE) {
            compromisedCredentialCount = getPasswordCheck().getCompromisedCredentialsCount();
            checkTimestamp = getPasswordCheck().getLastCheckTimestamp();
            header.set(SHOW_CHECK_SUBTITLE, true);

            // If a check was just completed, record some metrics.
            if (oldStatus == PasswordCheckUIStatus.RUNNING) {
                PasswordCheckMetricsRecorder.recordCompromisedCredentialsCountAfterCheck(
                        compromisedCredentialCount);
            }
        }
        header.set(CHECK_TIMESTAMP, checkTimestamp);
        header.set(COMPROMISED_CREDENTIALS_COUNT, compromisedCredentialCount);

        if (items.size() == 0) {
            items.add(new ListItem(PasswordCheckProperties.ItemType.HEADER, header));
        }
    }

    @Override
    public void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1;
        assert alreadyProcessed >= 0;
        assert remainingInQueue >= 0;

        PropertyModel header = items.get(0).model;
        if (header.get(CHECK_STATUS) != PasswordCheckUIStatus.RUNNING) {
            mLastStatusUpdate = System.currentTimeMillis();
            header.set(CHECK_STATUS, PasswordCheckUIStatus.RUNNING);
        }
        header.set(
                CHECK_PROGRESS, new Pair<>(alreadyProcessed, alreadyProcessed + remainingInQueue));
        header.set(CHECK_TIMESTAMP, null);
        header.set(COMPROMISED_CREDENTIALS_COUNT, null);
    }

    @Override
    public void onEdit(CompromisedCredential credential, Context context) {
        PasswordCheckMetricsRecorder.recordUiUserAction(
                PasswordCheckUserAction.EDIT_PASSWORD_CLICK);
        mDelegate.onEditCredential(credential, context);
    }

    @Override
    public void onRemove(CompromisedCredential credential) {
        PasswordCheckMetricsRecorder.recordUiUserAction(
                PasswordCheckUserAction.DELETE_PASSWORD_CLICK);
        mModel.set(DELETION_ORIGIN, credential.getDisplayOrigin());
        mModel.set(
                DELETION_CONFIRMATION_HANDLER,
                new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        PasswordCheckMetricsRecorder.recordUiUserAction(
                                PasswordCheckUserAction.DELETED_PASSWORD);
                        PasswordCheckMetricsRecorder.recordCheckResolutionAction(
                                PasswordCheckResolutionAction.DELETED_PASSWORD, credential);
                        if (which != AlertDialog.BUTTON_POSITIVE) return;
                        mDelegate.removeCredential(credential);
                        mModel.set(DELETION_CONFIRMATION_HANDLER, null);
                        mModel.set(DELETION_ORIGIN, null);
                    }

                    @Override
                    public void onDismiss() {
                        mModel.set(DELETION_CONFIRMATION_HANDLER, null);
                    }
                });
    }

    @Override
    public void onView(CompromisedCredential credential) {
        PasswordCheckMetricsRecorder.recordUiUserAction(
                PasswordCheckUserAction.VIEW_PASSWORD_CLICK);
        if (!mReauthenticationHelper.canReauthenticate()) {
            mReauthenticationHelper.showScreenLockToast(ReauthReason.VIEW_PASSWORD);
            return;
        }

        mReauthenticationHelper.reauthenticate(
                ReauthReason.VIEW_PASSWORD,
                reauthSucceeded -> {
                    if (reauthSucceeded) {
                        PasswordCheckMetricsRecorder.recordUiUserAction(
                                PasswordCheckUserAction.VIEWED_PASSWORD);
                        mModel.set(VIEW_CREDENTIAL, credential);
                        mModel.set(
                                VIEW_DIALOG_HANDLER,
                                new PasswordCheckViewDialogFragment.Handler() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        mModel.set(VIEW_CREDENTIAL, null);
                                        mModel.set(VIEW_DIALOG_HANDLER, null);
                                    }

                                    @Override
                                    public void onDismiss() {
                                        mModel.set(VIEW_DIALOG_HANDLER, null);
                                    }
                                });
                    }
                });
    }

    @Override
    public void onChangePasswordButtonClick(CompromisedCredential credential) {
        PasswordCheckMetricsRecorder.recordUiUserAction(PasswordCheckUserAction.CHANGE_PASSWORD);
        PasswordCheckMetricsRecorder.recordCheckResolutionAction(
                PasswordCheckResolutionAction.OPENED_SITE, credential);
        mCctIsOpened = true;
        mChangePasswordDelegate.launchAppOrCctWithChangePasswordUrl(credential);
    }

    private void updateStatusHeaderWhenCredentialsChange() {
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1;

        PropertyModel header = items.get(0).model;
        Integer compromisedCredentialsCount = getPasswordCheck().getCompromisedCredentialsCount();
        if (header.get(CHECK_STATUS) == PasswordCheckUIStatus.IDLE) {
            header.set(COMPROMISED_CREDENTIALS_COUNT, compromisedCredentialsCount);
        }
        header.set(
                SHOW_CHECK_SUBTITLE,
                compromisedCredentialsCount > 0
                        || header.get(CHECK_STATUS) == PasswordCheckUIStatus.IDLE);
    }

    public void stopCheck() {
        PasswordCheck check = PasswordCheckFactory.getPasswordCheckInstance();
        if (check == null) return;
        if (isCheckRunning()) {
            PasswordCheckMetricsRecorder.recordUiUserAction(PasswordCheckUserAction.CANCEL_CHECK);
        }
        check.stopCheck();
    }

    private void startCheckManually() {
        PasswordCheckMetricsRecorder.recordUiUserAction(
                PasswordCheckUserAction.START_CHECK_MANUALLY);
        getPasswordCheck().startCheck();
    }

    private PasswordCheck getPasswordCheck() {
        PasswordCheck passwordCheck = PasswordCheckFactory.getOrCreate();
        assert passwordCheck != null : "Password Check UI component needs native counterpart!";
        return passwordCheck;
    }

    private boolean isCheckRunning() {
        return mModel.get(ITEMS).get(0) != null
                && mModel.get(ITEMS).get(0).model.get(CHECK_STATUS)
                        == PasswordCheckUIStatus.RUNNING;
    }

    private boolean shouldDelayStatusChange(
            @PasswordCheckUIStatus int newStatus, long currentTime) {
        ListModel<ListItem> items = mModel.get(ITEMS);
        return items.size() > 0
                && items.get(0).model.get(CHECK_STATUS) == PasswordCheckUIStatus.RUNNING
                && newStatus != PasswordCheckUIStatus.RUNNING
                && mLastStatusUpdate + sStatusUpdateDelayMillis > currentTime;
    }

    private ListItem createEntryForCredential(CompromisedCredential credential) {
        PropertyModel credentialModel =
                new PropertyModel.Builder(
                                PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, credential)
                        .with(
                                HAS_MANUAL_CHANGE_BUTTON,
                                mChangePasswordDelegate.canManuallyChangeCredential(credential))
                        .with(CREDENTIAL_HANDLER, this)
                        .build();
        mIconHelper.getLargeIcon(
                credential,
                (faviconOrFallback) -> {
                    credentialModel.set(FAVICON_OR_FALLBACK, faviconOrFallback);
                });
        return new ListItem(
                PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL, credentialModel);
    }

    private void sortCredentials(List<CompromisedCredential> credentials) {
        if (mPreCheckSet == null) {
            mPreCheckSet = new HashSet<>(credentials);
        }

        Collections.sort(
                credentials,
                (CompromisedCredential lhs, CompromisedCredential rhs) -> {
                    // Phished credentials should always appear first.
                    if (lhs.isOnlyPhished() != rhs.isOnlyPhished()) {
                        return lhs.isOnlyPhished() ? -1 : 1;
                    }

                    boolean lhsInitial = mPreCheckSet.contains(lhs);
                    boolean rhsInitial = mPreCheckSet.contains(rhs);
                    // If one is the in initial set and the other one isn't, then the credential in
                    // the initial set goes first.
                    if (lhsInitial != rhsInitial) {
                        return lhsInitial ? -1 : 1;
                    }

                    // If they are both in the initial set, the most recent credential should appear
                    // first.
                    if (lhsInitial
                            && rhsInitial
                            && lhs.getCreationTime() != rhs.getCreationTime()) {
                        return -Long.compare(lhs.getCreationTime(), rhs.getCreationTime());
                    }

                    // If they both are not in the initial set, the older credential should appear
                    // first.
                    if (!lhsInitial
                            && !rhsInitial
                            && lhs.getCreationTime() != rhs.getCreationTime()) {
                        return Long.compare(lhs.getCreationTime(), rhs.getCreationTime());
                    }

                    // In case of creation time equality, order alphabetically (first by origin,
                    // then by username), so that the list remains stable.
                    int originComparisonResult =
                            lhs.getDisplayOrigin().compareTo(rhs.getDisplayOrigin());
                    int usernameComparisonResult =
                            lhs.getDisplayUsername().compareTo(rhs.getDisplayUsername());
                    return originComparisonResult == 0
                            ? usernameComparisonResult
                            : originComparisonResult;
                });
    }

    @VisibleForTesting
    protected static void setStatusUpdateDelayMillis(long statusUpdateDelayMillis) {
        sStatusUpdateDelayMillis = statusUpdateDelayMillis;
    }
}
