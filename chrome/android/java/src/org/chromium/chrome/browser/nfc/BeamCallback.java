// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.nfc;

import android.app.Activity;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter.CreateNdefMessageCallback;
import android.nfc.NfcAdapter.OnNdefPushCompleteCallback;
import android.nfc.NfcEvent;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.ui.widget.Toast;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Beam callback that gets passed to Android to get triggered when devices are tapped to
 * each other.
 */
class BeamCallback implements CreateNdefMessageCallback, OnNdefPushCompleteCallback {

    private static class Status {
        public final Integer errorStrID;
        public final String result;

        Status(Integer errorStrID) {
            assert errorStrID != null;
            this.errorStrID = errorStrID;
            this.result = null;
        }

        Status(String result) {
            assert result != null;
            this.result = result;
            this.errorStrID = null;
        }
    }

    // Arbitrarily chosen interval to delay toast to allow NFC animations to finish
    // and our app to return to foreground.
    private static final int TOAST_ERROR_DELAY_MS = 400;

    private final Activity mActivity;
    private final BeamProvider mProvider;

    // We use this to delay the error message in ICS because it would be hidden behind
    // the system beam overlay. It is only accessed by the NFC thread
    private Runnable mErrorRunnableIfBeamSent;

    BeamCallback(Activity activity, BeamProvider provider) {
        mActivity = activity;
        mProvider = provider;
    }

    @Override
    public NdefMessage createNdefMessage(NfcEvent event) {
        // Default status is an error
        Status status = new Status(R.string.nfc_beam_error_bad_url);
        try {
            status = ThreadUtils.runOnUiThread(new Callable<Status>() {
                @Override
                public Status call() {
                    String url = mProvider.getTabUrlForBeam();
                    if (url == null) return new Status(R.string.nfc_beam_error_overlay_active);
                    if (!isValidUrl(url)) return new Status(R.string.nfc_beam_error_bad_url);
                    return new Status(url);
                }
            }).get(2000, TimeUnit.MILLISECONDS); // Arbitrarily chosen timeout for query.
        } catch (TimeoutException e) {
            // Squelch this exception, we'll treat it as a bad tab
        } catch (ExecutionException e) {
            // And this
        } catch (InterruptedException e) {
            // And squelch this one too
        }

        if (status.errorStrID != null) {
            onInvalidBeam(status.errorStrID);
            return null;
        }

        RecordUserAction.record("MobileBeamCallbackSuccess");
        mErrorRunnableIfBeamSent = null;
        return new NdefMessage(new NdefRecord[] {NdefRecord.createUri(status.result)});
    }

    /**
     * Trigger an error about NFC if we don't want to send anything. Also
     * records a UMA stat. On ICS we only show the error if they attempt to
     * beam, since the recipient will receive the market link. On JB we'll
     * always show the error, since the beam animation won't trigger, which
     * could be confusing to the user.
     *
     * @param errorStringId The resid of the string to display as error.
     */
    private void onInvalidBeam(final int errorStringId) {
        RecordUserAction.record("MobileBeamInvalidAppState");
        Runnable errorRunnable = new Runnable() {
            @Override
            public void run() {
                Toast.makeText(mActivity, errorStringId, Toast.LENGTH_SHORT).show();
            }
        };
        ThreadUtils.runOnUiThread(errorRunnable);
    }

    @Override
    public void onNdefPushComplete(NfcEvent event) {
        if (mErrorRunnableIfBeamSent != null) {
            Handler h = new Handler(Looper.getMainLooper());
            h.postDelayed(mErrorRunnableIfBeamSent, TOAST_ERROR_DELAY_MS);
            mErrorRunnableIfBeamSent = null;
        }
    }

    /**
     * @return Whether given URL is valid and sharable via Beam.
     */
    private static boolean isValidUrl(String url) {
        if (TextUtils.isEmpty(url)) return false;
        try {
            String urlProtocol = (new URL(url)).getProtocol();
            return (UrlConstants.HTTP_SCHEME.equals(urlProtocol)
                    || UrlConstants.HTTPS_SCHEME.equals(urlProtocol));
        } catch (MalformedURLException e) {
            return false;
        }
    }
}
