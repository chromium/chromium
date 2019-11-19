// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.sync.Passphrase;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.text.DateFormat;
import java.util.Date;
import java.util.List;

/**
 * Dialog to ask the user select what type of password to use for encryption.
 */
public class PassphraseTypeDialogFragment extends DialogFragment implements
        DialogInterface.OnClickListener, OnItemClickListener {
    private static final String TAG = "PassphraseTypeDialogFragment";

    public interface Listener { void onPassphraseTypeSelected(@Passphrase.Type int type); }

    private String[] getDisplayNames(List<Integer /* @Passphrase.Type */> passphraseTypes) {
        String[] displayNames = new String[passphraseTypes.size()];
        for (int i = 0; i < displayNames.length; i++) {
            displayNames[i] = textForPassphraseType(passphraseTypes.get(i));
        }
        return displayNames;
    }

    private String textForPassphraseType(@Passphrase.Type int type) {
        switch (type) {
            case Passphrase.Type.IMPLICIT: // Intentional fall through.
            case Passphrase.Type.KEYSTORE:
                return getString(R.string.sync_passphrase_type_keystore);
            case Passphrase.Type.FROZEN_IMPLICIT:
                String passphraseDate = getPassphraseDateStringFromArguments();
                String frozenPassphraseString = getString(R.string.sync_passphrase_type_frozen);
                return String.format(frozenPassphraseString, passphraseDate);
            case Passphrase.Type.CUSTOM:
                return getString(R.string.sync_passphrase_type_custom);
            default:
                return "";
        }
    }

    private Adapter createAdapter(@Passphrase.Type int currentType) {
        List<Integer /* @Passphrase.Type */> passphraseTypes =
                Passphrase.getVisibleTypes(currentType);
        return new Adapter(passphraseTypes, getDisplayNames(passphraseTypes));
    }

    /**
     * The adapter for our ListView; only visible for testing purposes.
     */
    @VisibleForTesting
    public class Adapter extends ArrayAdapter<String> {
        private final List<Integer /* @PassphraseType */> mPassphraseTypes;

        /**
         * Do not call this constructor directly. Instead use
         * {@link PassphraseTypeDialogFragment#createAdapter}.
         */
        private Adapter(
                List<Integer /* @Passphrase.Type */> passphraseTypes, String[] displayStrings) {
            super(getActivity(), R.layout.passphrase_type_item, displayStrings);
            mPassphraseTypes = passphraseTypes;
        }

        @Override
        public boolean hasStableIds() {
            return true;
        }

        @Override
        public long getItemId(int position) {
            return getType(position);
        }

        public @Passphrase.Type int getType(int position) {
            return mPassphraseTypes.get(position);
        }

        public int getPositionForType(@Passphrase.Type int type) {
            return mPassphraseTypes.indexOf(type);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            CheckedTextView view = (CheckedTextView) super.getView(position, convertView, parent);
            @Passphrase.Type
            int positionType = getType(position);
            @Passphrase.Type
            int currentType = getCurrentTypeFromArguments();
            List<Integer /* @Passphrase.Type */> allowedTypes = Passphrase.getAllowedTypes(
                    currentType, getIsEncryptEverythingAllowedFromArguments());

            // Set the item to checked it if it is the currently selected encryption type.
            view.setChecked(positionType == currentType);
            // Allow user to click on enabled types for the current type.
            view.setEnabled(allowedTypes.contains(positionType));
            return view;
        }
    }

    /**
     * This argument should contain a single value of type {@link Passphrase.Type}.
     */
    private static final String ARG_CURRENT_TYPE = "arg_current_type";

    private static final String ARG_PASSPHRASE_TIME = "arg_passphrase_time";

    private static final String ARG_IS_ENCRYPT_EVERYTHING_ALLOWED =
            "arg_is_encrypt_everything_allowed";

    public static PassphraseTypeDialogFragment create(@Passphrase.Type int currentType,
            long passphraseTime, boolean isEncryptEverythingAllowed) {
        PassphraseTypeDialogFragment dialog = new PassphraseTypeDialogFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_CURRENT_TYPE, currentType);
        args.putLong(ARG_PASSPHRASE_TIME, passphraseTime);
        args.putBoolean(ARG_IS_ENCRYPT_EVERYTHING_ALLOWED, isEncryptEverythingAllowed);
        dialog.setArguments(args);
        return dialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.sync_passphrase_types, null);

        // Configure the passphrase type list
        ListView list = (ListView) v.findViewById(R.id.passphrase_types);

        @Passphrase.Type
        int currentType = getCurrentTypeFromArguments();

        // Configure the hint to reset the passphrase settings
        // Only show this hint if encryption has been set to use sync passphrase
        if (currentType == Passphrase.Type.CUSTOM) {
            TextViewWithClickableSpans instructionsView =
                    new TextViewWithClickableSpans(getActivity());
            instructionsView.setPadding(0,
                    getResources().getDimensionPixelSize(
                            R.dimen.sync_passphrase_type_instructions_padding),
                    0, 0);
            instructionsView.setMovementMethod(LinkMovementMethod.getInstance());
            instructionsView.setText(getResetText());
            list.addFooterView(instructionsView);
        }

        Adapter adapter = createAdapter(currentType);
        list.setAdapter(adapter);
        list.setId(R.id.passphrase_type_list);
        list.setOnItemClickListener(this);
        list.setDividerHeight(0);
        list.setSelection(adapter.getPositionForType(currentType));

        // Create and return the dialog
        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setNegativeButton(R.string.cancel, this)
                .setTitle(R.string.sync_passphrase_type_title)
                .setView(v)
                .create();
    }

    private SpannableString getResetText() {
        final Context context = getActivity();
        return SpanApplier.applySpans(
                context.getString(R.string.sync_passphrase_encryption_reset_instructions),
                new SpanInfo("<resetlink>", "</resetlink>", new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        Uri syncDashboardUrl = Uri.parse(ChromeStringConstants.SYNC_DASHBOARD_URL);
                        Intent intent = new Intent(Intent.ACTION_VIEW, syncDashboardUrl);
                        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
                        IntentUtils.safePutBinderExtra(
                                intent, CustomTabsIntent.EXTRA_SESSION, null);
                        context.startActivity(intent);
                    }
                }));
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == DialogInterface.BUTTON_NEGATIVE) {
            dismiss();
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long typeId) {
        @Passphrase.Type
        int currentType = getCurrentTypeFromArguments();
        // We know that typeId conversion from long to int is safe, because it represents very
        // small enum values.
        @Passphrase.Type
        int type = (int) typeId;
        boolean isEncryptEverythingAllowed = getIsEncryptEverythingAllowedFromArguments();
        if (Passphrase.getAllowedTypes(currentType, isEncryptEverythingAllowed).contains(type)) {
            if (type != currentType) {
                Listener listener = (Listener) getTargetFragment();
                listener.onPassphraseTypeSelected(type);
            }
            dismiss();
        }
    }

    @VisibleForTesting
    public @Passphrase.Type int getCurrentTypeFromArguments() {
        // NUM_ENTRIES is used to find when value doesn't exist.
        int currentType = getArguments().getInt(ARG_CURRENT_TYPE, Passphrase.Type.NUM_ENTRIES);
        if (currentType == Passphrase.Type.NUM_ENTRIES) {
            throw new IllegalStateException("Unable to find argument with current type.");
        }
        return currentType;
    }

    private String getPassphraseDateStringFromArguments() {
        long passphraseTime = getArguments().getLong(ARG_PASSPHRASE_TIME);
        DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
        return df.format(new Date(passphraseTime));
    }

    private boolean getIsEncryptEverythingAllowedFromArguments() {
        return getArguments().getBoolean(ARG_IS_ENCRYPT_EVERYTHING_ALLOWED);
    }
}
