// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Dialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Dialog to ask to user to enter their sync passphrase. */
public class PassphraseDialogFragment extends DialogFragment implements OnClickListener {
    private static final String TAG = "Sync_UI";

    /** A delegate for passphrase events/dependencies. */
    public interface Delegate {
        /**
         * @return whether passphrase was valid.
         */
        boolean onPassphraseEntered(String passphrase);

        void onPassphraseCanceled();

        /** Return the Profile associated with the passphrase. */
        Profile getProfile();
    }

    private EditText mPassphraseEditText;
    private TextView mVerifyingTextView;

    private Drawable mOriginalBackground;
    private Drawable mErrorBackground;

    /** Create a new instanceof of {@link PassphraseDialogFragment} and set its arguments. */
    public static PassphraseDialogFragment newInstance(Fragment target) {
        PassphraseDialogFragment dialog = new PassphraseDialogFragment();
        if (target != null) {
            dialog.setTargetFragment(target, -1);
        }
        return dialog;
    }

    private Profile getProfile() {
        Profile profile = getDelegate().getProfile();
        assert profile != null : "Attempting to use PassphraseDialogFragment with a null profile";
        // TODO(crbug/327687076): Remove the following profile fallback assuming no asserts are
        //                        triggered for the above profile assert.
        return profile == null ? ProfileManager.getLastUsedRegularProfile() : profile;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        assert SyncServiceFactory.getForProfile(getProfile()) != null;

        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.sync_enter_passphrase, null);

        TextView promptText = v.findViewById(R.id.prompt_text);
        promptText.setText(getPromptText());
        promptText.setMovementMethod(LinkMovementMethod.getInstance());

        TextView resetText = v.findViewById(R.id.reset_text);
        resetText.setText(getResetText());
        resetText.setMovementMethod(LinkMovementMethod.getInstance());
        resetText.setVisibility(View.VISIBLE);

        mVerifyingTextView = v.findViewById(R.id.verifying);

        mPassphraseEditText = v.findViewById(R.id.passphrase);
        mPassphraseEditText.setOnEditorActionListener(
                new OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        if (actionId == EditorInfo.IME_ACTION_NEXT) {
                            handleSubmit();
                        }
                        return false;
                    }
                });

        // Create a new background Drawable for the passphrase EditText to use when the user has
        // entered an invalid potential password.
        // https://crbug.com/602943 was caused by modifying the Drawable from getBackground()
        // without taking a copy.
        mOriginalBackground = mPassphraseEditText.getBackground();
        mErrorBackground = mOriginalBackground.getConstantState().newDrawable();
        mErrorBackground
                .mutate()
                .setColorFilter(
                        getContext().getColor(R.color.input_underline_error_color),
                        PorterDuff.Mode.SRC_IN);

        final AlertDialog d =
                new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(v)
                        .setPositiveButton(
                                R.string.submit,
                                new Dialog.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface d, int which) {
                                        // We override the onclick. This is a hack to not dismiss
                                        // the dialog after click of OK and instead dismiss it after
                                        // confirming the passphrase is correct.
                                    }
                                })
                        .setNegativeButton(R.string.cancel, this)
                        .setTitle(R.string.sync_enter_passphrase_title)
                        .create();

        d.getDelegate().setHandleNativeActionModesEnabled(false);
        d.setOnShowListener(
                new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface dialog) {
                        Button b = d.getButton(AlertDialog.BUTTON_POSITIVE);
                        b.setOnClickListener(
                                new View.OnClickListener() {
                                    @Override
                                    public void onClick(View view) {
                                        handleSubmit();
                                    }
                                });
                    }
                });
        return d;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_NEGATIVE) {
            handleCancel();
        }
    }

    @Override
    public void onResume() {
        resetPassphraseBoxColor();
        super.onResume();
    }

    private SpannableString getPromptText() {
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        String accountName =
                getString(R.string.sync_account_info, syncService.getAccountInfo().getEmail())
                        + "\n\n";
        return new SpannableString(
                accountName + getString(R.string.sync_enter_passphrase_body_with_email));
    }

    private SpannableString getResetText() {
        return SpanApplier.applySpans(
                getString(R.string.sync_passphrase_recover),
                new SpanInfo(
                        "BEGIN_LINK",
                        "END_LINK",
                        new ClickableSpan() {
                            @Override
                            public void onClick(View view) {
                                SyncSettingsUtils.openSyncDashboard(getActivity());
                            }
                        }));
    }

    private void handleCancel() {
        getDelegate().onPassphraseCanceled();
    }

    private void handleSubmit() {
        resetPassphraseBoxColor();
        mVerifyingTextView.setText(R.string.sync_verifying);

        String passphrase = mPassphraseEditText.getText().toString();
        boolean success = getDelegate().onPassphraseEntered(passphrase);
        if (!success) {
            invalidPassphrase();
        }
    }

    private Delegate getDelegate() {
        Fragment target = getTargetFragment();
        if (target instanceof Delegate) {
            return (Delegate) target;
        }
        return (Delegate) getActivity();
    }

    /** Notify this fragment that the passphrase the user entered is incorrect. */
    private void invalidPassphrase() {
        mVerifyingTextView.setText(R.string.sync_passphrase_incorrect);
        mVerifyingTextView.setTextColor(getContext().getColor(R.color.input_underline_error_color));

        mPassphraseEditText.setBackground(mErrorBackground);
    }

    private void resetPassphraseBoxColor() {
        mPassphraseEditText.setBackground(mOriginalBackground);
    }
}
