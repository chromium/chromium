// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.app.Activity;
import android.app.KeyguardManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationManagerCompat;
import androidx.fragment.app.Fragment;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/** A fragment that provides a UI for various caBLE v2 actions. */
public class CableAuthenticatorUI extends Fragment implements OnClickListener, FidoIntentSender {
    private static final String TAG = "CableAuthenticatorUI";

    // ENABLE_BLUETOOTH_REQUEST_CODE is a random int used to identify responses
    // to a request to enable Bluetooth. (Request codes can only be 16-bit.)
    private static final int ENABLE_BLUETOOTH_REQUEST_CODE = 64907;

    // PLAY_SERVICES_REQUEST_CODE is a random int used to identify responses
    // to a request to Play Services. (Request codes can only be 16-bit.)
    private static final int PLAY_SERVICES_REQUEST_CODE = 13466;

    // BLE_SCREEN_DELAY_SECS is the number of seconds that the screen for BLE
    // enabling will show before the request to actually enable BLE (which
    // causes Android to draw on top of it) is made.
    private static final int BLE_SCREEN_DELAY_SECS = 2;

    private static final String FCM_EXTRA = "org.chromium.chrome.modules.cablev2_authenticator.FCM";
    private static final String EVENT_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.EVENT";
    private static final String NETWORK_CONTEXT_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String SECRET_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";
    private static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";
    private static final String QR_EXTRA = "org.chromium.chrome.browser.webauth.authenticator.QR";

    // These entries duplicate some of the enum values from
    // device::cablev2::authenticator::Platform::Error. They must be kept in
    // sync with the C++ side because C++ communicates these values to this
    // code.
    private static final int ERROR_NONE = 0;
    private static final int ERROR_UNEXPECTED_EOF = 100;
    private static final int ERROR_NO_SCREENLOCK = 110;
    private static final int ERROR_NO_BLUETOOTH_PERMISSION = 111;
    private static final int ERROR_AUTHENTICATOR_SELECTION_RECEIVED = 114;
    private static final int ERROR_DISCOVERABLE_CREDENTIALS_REQUEST = 115;

    // These entries duplicate some of the enum values from
    // `CableV2MobileEvent`. The C++ enum is the source of truth for these
    // values.

    private enum Mode {
        QR, // QR code scanned by external app.
        FCM, // Triggered by user selecting notification; handshake already running.
        SERVER_LINK, // Triggered by GMSCore forwarding from GAIA.
    }

    private Mode mMode;

    // State enumerates the different states of the UI. Apart from transitions to `ERROR`, changes
    // to the state are handled by `onEvent`.
    private enum State {
        START,
        QR_CONFIRM,
        CHECK_SCREENLOCK,
        NO_SCREENLOCK,
        START_MODE,
        ENABLE_BLUETOOTH,
        ENABLE_BLUETOOTH_WAITING,
        ENABLE_BLUETOOTH_PENDING,
        ENABLE_BLUETOOTH_PERMISSION_REQUESTED,
        REQUEST_BLUETOOTH_ENABLE,
        BLUETOOTH_ENABLED,
        BLUETOOTH_ADVERTISE_PERMISSION_REQUESTED,
        BLUETOOTH_READY,
        RUNNING_BLE,
        ERROR,
    }

    private State mState;

    // mErrorCode contains a value of the authenticator::Platform::Error
    // enumeration when |mState| is |ERROR|.
    private int mErrorCode;

    // Event enumerates outside events that can cause a state transition in `onEvent`.
    private enum Event {
        NONE,
        RESUMED,
        BLE_ENABLED,
        PERMISSIONS_GRANTED,
        QR_ALLOW_BUTTON_CLICKED,
        QR_DENY_BUTTON_CLICKED,
        TIMEOUT_COMPLETE;
    }

    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;
    // mViewsCreated is set to true after `onCreateView` has been called, which sets values for all
    // the `View`-typed members of this object. Prior to this UI updates are suppressed.
    private boolean mViewsCreated;
    // mActivityStarted is set to true by `onResume`. Some event transitions are suppressed until
    // this flag has been set.
    private boolean mActivityStarted;
    // mFidoCallback holds the callback from `Fido2CredentialRequest` while a
    // request to Play Services is outstanding.
    private Callback<Pair<Integer, Intent>> mFidoCallback;

    // These are top-level views that can fill this activity.
    private View mErrorView;
    private View mSpinnerView;
    private View mBLEEnableView;
    private View mQRConfirmView;

    // mCurrentView is one of the above views depending on which is currently showing.
    private View mCurrentView;

    // These are views within the top-level activity.
    private View mErrorCloseButton;
    private View mErrorSettingsButton;
    private View mQRAllowButton;
    private View mQRRejectButton;
    private TextView mStatusText;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Context context = getContext();

        Bundle arguments = getArguments();
        final byte[] serverLink = arguments.getByteArray(SERVER_LINK_EXTRA);
        final byte[] fcmEvent = arguments.getByteArray(EVENT_EXTRA);
        final Uri qrFromArguments = (Uri) arguments.getParcelable(QR_EXTRA);
        final String qrURI = qrFromArguments == null ? null : qrFromArguments.toString();
        if (arguments.getBoolean(FCM_EXTRA)) {
            mMode = Mode.FCM;
        } else if (serverLink != null) {
            mErrorCode = CableAuthenticator.validateServerLinkData(serverLink);
            if (mErrorCode != ERROR_NONE) {
                mState = State.ERROR;
                return;
            }

            mMode = Mode.SERVER_LINK;
        } else if (qrURI != null) {
            mErrorCode = CableAuthenticator.validateQRURI(qrURI);
            if (mErrorCode != ERROR_NONE) {
                mState = State.ERROR;
                return;
            }

            mMode = Mode.QR;
        } else {
            assert false;
            getActivity().finish();
        }

        Log.i(TAG, "Starting in mode " + mMode.toString());

        final long networkContext = arguments.getLong(NETWORK_CONTEXT_EXTRA);
        final long registration = arguments.getLong(REGISTRATION_EXTRA);
        final byte[] secret = arguments.getByteArray(SECRET_EXTRA);

        mPermissionDelegate =
                new ActivityAndroidPermissionDelegate(
                        new WeakReference<Activity>((Activity) context));
        mAuthenticator =
                new CableAuthenticator(
                        getContext(),
                        this,
                        networkContext,
                        registration,
                        secret,
                        mMode == Mode.FCM,
                        serverLink,
                        fcmEvent,
                        qrURI);

        mState = State.START;
        onEvent(Event.NONE);
    }

    private void onEvent(Event event) {
        if (mAuthenticator == null) {
            // Activity was stopped before this event happened. Ignore.
            return;
        }

        while (true) {
            final State stateBefore = mState;
            updateUiForState();

            switch (mState) {
                case START:
                    assert event == Event.NONE;

                    // QR mode requires a confirmation screen first. All other modes move
                    // on to the screen-lock check.
                    if (mMode == Mode.QR) {
                        mState = State.QR_CONFIRM;
                    } else {
                        mState = State.CHECK_SCREENLOCK;
                    }
                    break;

                case QR_CONFIRM:
                    if (event == Event.QR_ALLOW_BUTTON_CLICKED) {
                        ViewGroup top = (ViewGroup) getView();
                        boolean link =
                                ((CheckBox) top.findViewById(R.id.qr_link)).isChecked()
                                        && NotificationManagerCompat.from(getContext())
                                                .areNotificationsEnabled();
                        mAuthenticator.setQRLinking(link);
                        mState = State.CHECK_SCREENLOCK;
                        break;
                    } else if (event == Event.QR_DENY_BUTTON_CLICKED) {
                        getActivity().finish();
                        return;
                    }
                    return;

                case CHECK_SCREENLOCK:
                    // GMSCore will immediately fail all requests if a screenlock isn't
                    // configured, except for server-link because PaaSK is special.
                    // Outside of server-link, the device shouldn't have advertised itself
                    // via Sync, but it's possible for a request to come in soon after a
                    // screen lock was removed. When using a QR code it's always possible
                    // for the screen-lock to be missing.
                    if (mMode == Mode.SERVER_LINK || hasScreenLockConfigured(getContext())) {
                        mState = State.START_MODE;
                    } else {
                        mState = State.NO_SCREENLOCK;
                    }
                    break;

                case NO_SCREENLOCK:
                    if (event == Event.RESUMED && hasScreenLockConfigured(getContext())) {
                        // The user tapped the "Open settings" button on the error page
                        // explaining that no screen lock was set, and set a screen lock.
                        // The flow now continues based on `mMode`.
                        mState = State.START_MODE;
                        break;
                    }
                    return;

                case START_MODE:
                    switch (mMode) {
                        case SERVER_LINK:
                        case FCM:
                        case QR:
                            mState = State.ENABLE_BLUETOOTH;
                            break;
                    }
                    break;

                case ENABLE_BLUETOOTH:
                    if (BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser() != null) {
                        mState = State.BLUETOOTH_ENABLED;
                        break;
                    }

                    if (!mActivityStarted) {
                        return;
                    }

                    mState = State.ENABLE_BLUETOOTH_WAITING;
                    PostTask.postDelayedTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                onEvent(Event.TIMEOUT_COMPLETE);
                            },
                            BLE_SCREEN_DELAY_SECS * 1000);
                    break;

                case ENABLE_BLUETOOTH_WAITING:
                    if (event != Event.TIMEOUT_COMPLETE) {
                        return;
                    }

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                            && requestBluetoothPermissions()) {
                        mState = State.ENABLE_BLUETOOTH_PERMISSION_REQUESTED;
                        return;
                    }

                    mState = State.REQUEST_BLUETOOTH_ENABLE;
                    break;

                case ENABLE_BLUETOOTH_PERMISSION_REQUESTED:
                    if (event != Event.PERMISSIONS_GRANTED) {
                        return;
                    }
                    mState = State.REQUEST_BLUETOOTH_ENABLE;
                    break;

                case REQUEST_BLUETOOTH_ENABLE:
                    mState = State.ENABLE_BLUETOOTH_PENDING;
                    startActivityForResult(
                            new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
                            ENABLE_BLUETOOTH_REQUEST_CODE);
                    break;

                case ENABLE_BLUETOOTH_PENDING:
                    if (event != Event.BLE_ENABLED) {
                        return;
                    }
                    mState = State.BLUETOOTH_ENABLED;
                    break;

                case BLUETOOTH_ENABLED:
                    if (!mActivityStarted) {
                        return;
                    }

                    // In Android 12 and above there is a new BLUETOOTH_ADVERTISE runtime
                    // permission.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                            && requestBluetoothPermissions()) {
                        mState = State.BLUETOOTH_ADVERTISE_PERMISSION_REQUESTED;
                        return;
                    }

                    mState = State.BLUETOOTH_READY;
                    break;

                case BLUETOOTH_ADVERTISE_PERMISSION_REQUESTED:
                    if (event != Event.PERMISSIONS_GRANTED) {
                        return;
                    }
                    mState = State.BLUETOOTH_READY;
                    break;

                case BLUETOOTH_READY:
                    mAuthenticator.onTransportReady();
                    mState = State.RUNNING_BLE;
                    break;

                case RUNNING_BLE:
                    return;

                case ERROR:
                    if (event == Event.RESUMED
                            && mErrorCode == ERROR_NO_BLUETOOTH_PERMISSION
                            && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                            && haveBluetoothPermissions()) {
                        // The user navigated away and came back, but now we have the needed
                        // permission.
                        mState = State.ENABLE_BLUETOOTH;
                        break;
                    }

                    return;
            }

            Log.e(TAG, stateBefore.toString() + " -> " + mState.toString());

            if (stateBefore == mState) {
                // A state should either `return` to block for an event or else have updated
                // `mState`.
                assert false;
                break;
            }

            // An event shouldn't appear to have happened again for the next state.
            event = Event.NONE;
        }
    }

    /** Returns the {@link View} that should be showing, given {@link mState}. */
    private View getUiForState() {
        switch (mState) {
            case QR_CONFIRM:
                return mQRConfirmView;

            case NO_SCREENLOCK:
                fillOutErrorUI(ERROR_NO_SCREENLOCK);
                return mErrorView;

            case ENABLE_BLUETOOTH:
            case ENABLE_BLUETOOTH_WAITING:
            case ENABLE_BLUETOOTH_PENDING:
            case ENABLE_BLUETOOTH_PERMISSION_REQUESTED:
            case REQUEST_BLUETOOTH_ENABLE:
                return mBLEEnableView;

            case ERROR:
                fillOutErrorUI(mErrorCode);
                return mErrorView;

            default:
                return mSpinnerView;
        }
    }

    /** Updates the UI based on the value of {@link mState}. */
    private void updateUiForState() {
        if (!mViewsCreated) {
            // If {@link onCreateView} hasn't been called yet then there is no
            // UI to update. Instead {@link onCreateView} will set the initial
            // {@link View} based on {@link mState}.
            return;
        }

        View newView = getUiForState();
        if (newView == mCurrentView) {
            return;
        }

        ViewGroup top = (ViewGroup) getView();
        top.removeAllViews();
        mCurrentView = newView;
        top.addView(mCurrentView);
    }

    private View createSpinnerScreen(LayoutInflater inflater, ViewGroup container) {
        View v = inflater.inflate(R.layout.cablev2_spinner, container, false);
        mStatusText = v.findViewById(R.id.status_text);

        final AnimatedVectorDrawableCompat anim =
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.circle_loader_animation);
        // There is no way to make an animation loop. Instead it must be
        // manually started each time it completes.
        anim.registerAnimationCallback(
                new Animatable2Compat.AnimationCallback() {
                    @Override
                    public void onAnimationEnd(Drawable drawable) {
                        if (drawable != null) {
                            anim.start();
                        }
                    }
                });
        ((ImageView) v.findViewById(R.id.spinner)).setImageDrawable(anim);
        anim.start();

        return v;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mErrorView = inflater.inflate(R.layout.cablev2_error, container, false);
        mErrorCloseButton = mErrorView.findViewById(R.id.error_close);
        mErrorCloseButton.setOnClickListener(this);
        mErrorSettingsButton = mErrorView.findViewById(R.id.error_settings_button);
        mErrorSettingsButton.setOnClickListener(this);

        mSpinnerView = createSpinnerScreen(inflater, container);
        mBLEEnableView = inflater.inflate(R.layout.cablev2_ble_enable, container, false);

        mQRConfirmView = inflater.inflate(R.layout.cablev2_qr, container, false);
        mQRAllowButton = mQRConfirmView.findViewById(R.id.qr_connect);
        mQRAllowButton.setOnClickListener(this);
        mQRRejectButton = mQRConfirmView.findViewById(R.id.qr_reject);
        mQRRejectButton.setOnClickListener(this);

        mViewsCreated = true;

        getActivity().setTitle(R.string.cablev2_activity_title);
        ViewGroup top = new LinearLayout(getContext());
        mCurrentView = getUiForState();
        top.addView(mCurrentView);
        return top;
    }

    @Override
    public void onResume() {
        super.onResume();
        mActivityStarted = true;
        onEvent(Event.RESUMED);
    }

    // This class should not be reachable on Android versions < N (API level 24).
    @RequiresApi(24)
    private static boolean hasScreenLockConfigured(Context context) {
        KeyguardManager km = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        return km.isDeviceSecure();
    }

    @RequiresApi(31)
    private boolean haveBluetoothPermissions() {
        return getContext().checkSelfPermission(permission.BLUETOOTH_CONNECT)
                        == PackageManager.PERMISSION_GRANTED
                && getContext().checkSelfPermission(permission.BLUETOOTH_ADVERTISE)
                        == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Request Bluetooth permissions, if needed.
     *
     * @return true if permissions were requested.
     */
    @RequiresApi(31)
    private boolean requestBluetoothPermissions() {
        if (!haveBluetoothPermissions()) {
            if (shouldShowRequestPermissionRationale(permission.BLUETOOTH_CONNECT)
                    || shouldShowRequestPermissionRationale(permission.BLUETOOTH_ADVERTISE)) {
                // Since the user took explicit action to make a connection to their
                // computer, and there's a big "Enabling Bluetooth" or "Connecting to your computer"
                // in the background, the rationale should be clear. However, these functions must
                // always be called otherwise the permission will be automatically refused.
            }

            requestPermissions(
                    new String[] {permission.BLUETOOTH_CONNECT, permission.BLUETOOTH_ADVERTISE},
                    /* requestCode= */ 1);
            return true;
        }

        return false;
    }

    @Override
    public void onClick(View v) {
        if (v == mErrorCloseButton) {
            getActivity().finish();
        } else if (v == mErrorSettingsButton) {
            // Open the Settings screen for Chromium.
            Intent intent;
            if (mState == State.NO_SCREENLOCK) {
                intent = new Intent(android.app.admin.DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
            } else if (mState == State.ERROR && mErrorCode == ERROR_NO_BLUETOOTH_PERMISSION) {
                intent = new Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                intent.setData(
                        android.net.Uri.fromParts(
                                "package", BuildInfo.getInstance().packageName, null));
            } else {
                // Should never be reached. Button should not be shown unless
                // the error is known.
                intent = new Intent(android.provider.Settings.ACTION_SETTINGS);
            }
            startActivity(intent);
        } else if (v == mQRAllowButton) {
            onEvent(Event.QR_ALLOW_BUTTON_CLICKED);
        } else if (v == mQRRejectButton) {
            onEvent(Event.QR_DENY_BUTTON_CLICKED);
        }
    }

    void onStatus(int code) {
        switch (mMode) {
            case QR:
            case SERVER_LINK:
            case FCM:
                // These values must match up with the Status enum in v2_authenticator.h
                int id = -1;
                if (code == 1) {
                    if (mMode == Mode.SERVER_LINK) {
                        id = R.string.cablev2_serverlink_status_connecting;
                    } else {
                        id = R.string.cablev2_fcm_status_connecting;
                    }
                } else if (code == 2) {
                    id = R.string.cablev2_serverlink_status_connected;
                } else if (code == 3) {
                    id = R.string.cablev2_serverlink_status_processing;
                } else {
                    break;
                }

                mStatusText.setText(getResources().getString(id));
                break;
        }
    }

    /**
     * Called when camera permission has been requested and the user has resolved the permission
     * request.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        final boolean granted =
                grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;

        if (granted) {
            onEvent(Event.PERMISSIONS_GRANTED);
            return;
        }

        mState = State.ERROR;
        mErrorCode = ERROR_NO_BLUETOOTH_PERMISSION;
        updateUiForState();
    }

    @Override
    public void onStop() {
        super.onStop();

        if (mAuthenticator != null) {
            mAuthenticator.onActivityStop();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // Closing mAuthenticator is not done in |onStop| because Android can
        // generate spurious |onStop| calls when the activity is started from
        // a lock-screen notification.
        if (mAuthenticator != null) {
            mAuthenticator.close();
            mAuthenticator = null;
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (mAuthenticator == null) {
            return;
        }

        if (requestCode == PLAY_SERVICES_REQUEST_CODE) {
            mFidoCallback.onResult(new Pair(resultCode, data));
        } else if (requestCode == ENABLE_BLUETOOTH_REQUEST_CODE) {
            if (resultCode != Activity.RESULT_OK) {
                getActivity().finish();
                return;
            }
            onEvent(Event.BLE_ENABLED);
        } else {
            mAuthenticator.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
        mFidoCallback = callback;
        try {
            startIntentSenderForResult(
                    intent.getIntentSender(),
                    PLAY_SERVICES_REQUEST_CODE,
                    null, // fillInIntent,
                    0, // flagsMask,
                    0, // flagsValue,
                    0, // extraFlags,
                    Bundle.EMPTY);
        } catch (android.content.IntentSender.SendIntentException e) {
            Log.e(TAG, "SendIntentException", e);
            return false;
        }
        return true;
    }

    void onAuthenticatorConnected() {}

    void onAuthenticatorResult(CableAuthenticator.Result result) {
        getActivity()
                .runOnUiThread(
                        () -> {
                            int id = -1;
                            switch (result) {
                                case REGISTER_OK:
                                    id = R.string.cablev2_registration_succeeded;
                                    break;
                                case REGISTER_ERROR:
                                    id = R.string.cablev2_registration_failed;
                                    break;
                                case SIGN_OK:
                                    id = R.string.cablev2_sign_in_succeeded;
                                    break;
                                case SIGN_ERROR:
                                case OTHER:
                                    id = R.string.cablev2_sign_in_failed;
                                    break;
                            }
                            Toast.makeText(
                                            getActivity(),
                                            getResources().getString(id),
                                            Toast.LENGTH_SHORT)
                                    .show();
                            getActivity().finish();
                        });
    }

    /**
     * Called when a transaction has completed.
     *
     * @param ok true if the transaction completed successfully. Otherwise it
     *           indicates some form of error that could include tunnel server
     *           errors, handshake failures, etc.
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    void onComplete(boolean ok, int errorCode) {
        ThreadUtils.assertOnUiThread();

        if (ok) {
            getActivity().finish();
            return;
        }

        mState = State.ERROR;
        mErrorCode = errorCode;
        updateUiForState();
    }

    /**
     * Fills out the elements of |mErrorView| for the given error code.
     *
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    void fillOutErrorUI(int errorCode) {
        mErrorCloseButton = mErrorView.findViewById(R.id.error_close);
        mErrorCloseButton.setOnClickListener(this);
        mErrorSettingsButton = mErrorView.findViewById(R.id.error_settings_button);
        mErrorSettingsButton.setOnClickListener(this);

        final String packageLabel = BuildInfo.getInstance().hostPackageLabel;
        String desc;
        boolean settingsButtonVisible = false;
        switch (errorCode) {
            case ERROR_UNEXPECTED_EOF:
                desc = getResources().getString(R.string.cablev2_error_timeout);
                break;

            case ERROR_NO_BLUETOOTH_PERMISSION:
                desc =
                        getResources()
                                .getString(R.string.cablev2_error_ble_permission, packageLabel);
                settingsButtonVisible = true;
                break;

            case ERROR_NO_SCREENLOCK:
                desc = getResources().getString(R.string.cablev2_error_no_screenlock, packageLabel);
                settingsButtonVisible = true;
                break;

            case ERROR_AUTHENTICATOR_SELECTION_RECEIVED:
            case ERROR_DISCOVERABLE_CREDENTIALS_REQUEST:
                desc = getResources().getString(R.string.cablev2_error_disco_cred, packageLabel);
                break;

            default:
                TextView errorCodeTextView = mErrorView.findViewById(R.id.error_code);
                errorCodeTextView.setText(
                        getResources().getString(R.string.cablev2_error_code, errorCode));

                desc = getResources().getString(R.string.cablev2_error_generic);
                break;
        }

        ((View) mErrorView.findViewById(R.id.error_settings_button))
                .setVisibility(settingsButtonVisible ? View.VISIBLE : View.INVISIBLE);

        TextView descriptionTextView = mErrorView.findViewById(R.id.error_description);
        descriptionTextView.setText(desc);
    }
}
