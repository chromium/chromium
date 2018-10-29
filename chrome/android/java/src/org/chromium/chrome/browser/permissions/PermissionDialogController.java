// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.annotation.SuppressLint;
import android.content.DialogInterface;
import android.support.annotation.IntDef;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.List;

/**
 * Singleton instance which controls the display of modal permission dialogs. This class is lazily
 * initiated when getInstance() is first called.
 *
 * Unlike permission infobars, which stack on top of each other, only one permission dialog may be
 * visible on the screen at once. Any additional request for a modal permissions dialog is queued,
 * and will be displayed once the user responds to the current dialog.
 */
public class PermissionDialogController
        implements AndroidPermissionRequester.RequestDelegate, ModalDialogView.Controller {
    @IntDef({State.NOT_SHOWING, State.PROMPT_PENDING, State.PROMPT_OPEN, State.PROMPT_ACCEPTED,
            State.PROMPT_DENIED, State.REQUEST_ANDROID_PERMISSIONS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int NOT_SHOWING = 0;
        // We don't show prompts while Chrome Home is showing.
        int PROMPT_PENDING = 1;
        int PROMPT_OPEN = 2;
        int PROMPT_ACCEPTED = 3;
        int PROMPT_DENIED = 4;
        int REQUEST_ANDROID_PERMISSIONS = 5;
    }

    private PermissionDialogView mDialogView;
    private PermissionAppModalDialogView mAppModalDialogView;
    private PermissionDialogDelegate mDialogDelegate;
    private ModalDialogManager mModalDialogManager;
    private DialogInterface.OnClickListener mPositiveClickListener;
    private DialogInterface.OnClickListener mNegativeClickListener;
    private DialogInterface.OnDismissListener mDismissListener;

    // As the PermissionRequestManager handles queueing for a tab and only shows prompts for active
    // tabs, we typically only have one request. This class only handles multiple requests at once
    // when either:
    // 1) Multiple open windows request permissions due to Android split-screen
    // 2) A tab navigates or is closed while the Android permission request is open, and the
    // subsequent page requests a permission
    private List<PermissionDialogDelegate> mRequestQueue;

    /** The current state, whether we have a prompt showing and so on. */
    private @State int mState;

    // Static holder to ensure safe initialization of the singleton instance.
    private static class Holder {
        @SuppressLint("StaticFieldLeak")
        private static final PermissionDialogController sInstance =
                new PermissionDialogController();
    }

    public static PermissionDialogController getInstance() {
        return Holder.sInstance;
    }

    private PermissionDialogController() {
        mRequestQueue = new LinkedList<>();
        mState = State.NOT_SHOWING;
    }

    /**
     * Called by native code to create a modal permission dialog. The PermissionDialogController
     * will decide whether the dialog needs to be queued (because another dialog is on the screen)
     * or whether it can be shown immediately.
     */
    @CalledByNative
    private static void createDialog(PermissionDialogDelegate delegate) {
        PermissionDialogController.getInstance().queueDialog(delegate);
    }

    /**
     * Queues a modal permission dialog for display. If there are currently no dialogs on screen, it
     * will be displayed immediately. Otherwise, it will be displayed as soon as the user responds
     * to the current dialog.
     * @param context  The context to use to get the dialog layout.
     * @param delegate The wrapper for the native-side permission delegate.
     */
    private void queueDialog(PermissionDialogDelegate delegate) {
        mRequestQueue.add(delegate);
        delegate.setDialogController(this);
        scheduleDisplay();
    }

    private void scheduleDisplay() {
        if (mState == State.NOT_SHOWING && !mRequestQueue.isEmpty()) dequeueDialog();
    }

    @VisibleForTesting
    public PermissionDialogView getCurrentDialogForTesting() {
        return mDialogView;
    }

    @Override
    public void onAndroidPermissionAccepted() {
        assert mState == State.REQUEST_ANDROID_PERMISSIONS;

        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            mDialogDelegate.onAccept();
            destroyDelegate();
        }
        scheduleDisplay();
    }

    @Override
    public void onAndroidPermissionCanceled() {
        assert mState == State.REQUEST_ANDROID_PERMISSIONS;

        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            mDialogDelegate.onDismiss();
            destroyDelegate();
        }
        scheduleDisplay();
    }

    /**
     * Shows the dialog asking the user for a web API permission.
     */
    public void dequeueDialog() {
        assert mState == State.NOT_SHOWING;

        mDialogDelegate = mRequestQueue.remove(0);
        mState = State.PROMPT_PENDING;
        ChromeActivity activity = mDialogDelegate.getTab().getActivity();

        // It's possible for the activity to be null if we reach here just after the user
        // backgrounds the browser and cleanup has happened. In that case, we can't show a prompt,
        // so act as though the user dismissed it.
        if (activity == null) {
            // TODO(timloh): This probably doesn't work, as this happens synchronously when creating
            // the PermissionPromptAndroid, so the PermissionRequestManager won't be ready yet.
            mDialogDelegate.onDismiss();
            destroyDelegate();
            return;
        }

        // Suppress modals while Chrome Home is open. Eventually we will want to handle other cases
        // whereby the tab is obscured so modals don't pop up on top of (e.g.) the tab switcher or
        // the three-dot menu.
        final BottomSheet bottomSheet = activity.getBottomSheet();
        if (bottomSheet == null || !bottomSheet.isSheetOpen()) {
            showDialog();
        } else {
            bottomSheet.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(int reason) {
                    bottomSheet.removeObserver(this);
                    if (reason == BottomSheet.StateChangeReason.NAVIGATION) {
                        // Dismiss the prompt as it would otherwise be dismissed momentarily once
                        // the navigation completes.
                        // TODO(timloh): This logs a dismiss (and we also already logged a show),
                        // even though the user didn't see anything.
                        mDialogDelegate.onDismiss();
                        destroyDelegate();
                    } else {
                        showDialog();
                    }
                }
            });
        }
    }

    private void showDialog() {
        assert mState == State.PROMPT_PENDING;

        // The tab may have navigated or been closed while we were waiting for Chrome Home to close.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
            scheduleDisplay();
            return;
        }

        // Set the buttons to call the appropriate delegate methods. When the dialog is dismissed,
        // the delegate's native pointers are freed, and the next queued dialog (if any) is
        // displayed.
        mPositiveClickListener =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        assert mState == State.PROMPT_OPEN;
                        mState = State.PROMPT_ACCEPTED;
                    }
                };
        mNegativeClickListener =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        assert mState == State.PROMPT_OPEN;
                        mState = State.PROMPT_DENIED;
                    }
                };

        // Called when the dialog is dismissed. Interacting with either button in the dialog will
        // call this handler after the primary/secondary handler.
        mDismissListener =
                new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface dialog) {
                        mDialogView = null;
                        if (mDialogDelegate == null) {
                            // We get into here if a tab navigates or is closed underneath the
                            // prompt.
                            mState = State.NOT_SHOWING;
                            return;
                        }
                        if (mState == State.PROMPT_ACCEPTED) {
                            // Request Android permissions if necessary. This will call back into
                            // either onAndroidPermissionAccepted or onAndroidPermissionCanceled,
                            // which will schedule the next permission dialog. If it returns false,
                            // no system level permissions need to be requested, so just run the
                            // accept callback.
                            mState = State.REQUEST_ANDROID_PERMISSIONS;
                            if (!AndroidPermissionRequester.requestAndroidPermissions(
                                        mDialogDelegate.getTab(),
                                        mDialogDelegate.getContentSettingsTypes(),
                                        PermissionDialogController.this)) {
                                onAndroidPermissionAccepted();
                            }
                        } else {
                            // Otherwise, run the necessary delegate callback immediately and
                            // schedule the next dialog.
                            if (mState == State.PROMPT_DENIED) {
                                mDialogDelegate.onCancel();
                            } else {
                                assert mState == State.PROMPT_OPEN;
                                mDialogDelegate.onDismiss();
                            }
                            destroyDelegate();
                            scheduleDisplay();
                        }
                    }
                };

        if (useAppModalDialogView()) {
            mModalDialogManager = mDialogDelegate.getTab().getActivity().getModalDialogManager();
            mAppModalDialogView = PermissionAppModalDialogView.create(this, mDialogDelegate);
            mModalDialogManager.showDialog(
                    mAppModalDialogView, ModalDialogManager.ModalDialogType.APP);
        } else {
            mDialogView = new PermissionDialogView(mDialogDelegate);
            mDialogView.createView(
                    mPositiveClickListener, mNegativeClickListener, mDismissListener);
            mDialogView.show();
        }
        mState = State.PROMPT_OPEN;
    }

    public void dismissFromNative(PermissionDialogDelegate delegate) {
        if (mDialogDelegate == delegate) {
            // Some caution is required here to handle cases where the user actions or dismisses
            // the prompt at roughly the same time as native. Due to asynchronicity, this function
            // may be called after onClick and before onDismiss, or before both of those listeners.
            mDialogDelegate = null;
            if (mState == State.PROMPT_OPEN) {
                if (useAppModalDialogView()) {
                    mModalDialogManager.dismissDialog(mAppModalDialogView);
                } else {
                    mDialogView.dismiss();
                }
            } else {
                assert mState == State.PROMPT_PENDING || mState == State.REQUEST_ANDROID_PERMISSIONS
                        || mState == State.PROMPT_DENIED || mState == State.PROMPT_ACCEPTED;
            }
        } else {
            assert mRequestQueue.contains(delegate);
            mRequestQueue.remove(delegate);
        }
        delegate.destroy();
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        mDismissListener.onDismiss(null);
        mAppModalDialogView = null;
    }

    @Override
    public void onClick(@ModalDialogView.ButtonType int buttonType) {
        switch (buttonType) {
            case ModalDialogView.ButtonType.POSITIVE:
                mPositiveClickListener.onClick(null, 0);
                break;
            case ModalDialogView.ButtonType.NEGATIVE:
                mNegativeClickListener.onClick(null, 0);
                break;
            default:
                assert false : "Unexpected button pressed in dialog: " + buttonType;
        }
        mModalDialogManager.dismissDialog(mAppModalDialogView);
    }

    private void destroyDelegate() {
        mDialogDelegate.destroy();
        mDialogDelegate = null;
        mState = State.NOT_SHOWING;
    }

    private static boolean useAppModalDialogView() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.MODAL_PERMISSION_DIALOG_VIEW)
                || VrModuleProvider.getDelegate().isInVr();
    }
}
