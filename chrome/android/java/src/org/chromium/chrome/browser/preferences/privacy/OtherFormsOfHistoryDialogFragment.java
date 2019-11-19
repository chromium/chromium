// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Informs the user about the existence of other forms of browsing history.
 */
public class OtherFormsOfHistoryDialogFragment extends DialogFragment implements
        DialogInterface.OnClickListener {

    public static final String PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN =
            "org.chromium.chrome.browser.preferences.privacy."
            + "PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN";

    private static final String TAG = "OtherFormsOfHistoryDialogFragment";

    /**
     * Show the dialog.
     * @param activity The activity in which to show the dialog.
     */
    public void show(Activity activity) {
        show(activity.getFragmentManager(), TAG);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        super.onCreateDialog(savedInstanceState);
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View view = inflater.inflate(R.layout.other_forms_of_history_dialog, null);

        // Linkify the <link></link> span in the dialog text.
        TextView textView = (TextView) view.findViewById(R.id.text);
        final SpannableString textWithLink = SpanApplier.applySpans(textView.getText().toString(),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(getResources(), (widget) -> {
                            new TabDelegate(false /* incognito */)
                                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD_NOTICE,
                                            TabLaunchType.FROM_CHROME_UI);
                        })));

        textView.setText(textWithLink);
        textView.setMovementMethod(LinkMovementMethod.getInstance());

        // Construct the dialog.
        AlertDialog dialog =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                        .setView(view)
                        .setTitle(R.string.clear_browsing_data_history_dialog_title)
                        .setPositiveButton(R.string.ok_got_it, this)
                        .create();

        dialog.setCanceledOnTouchOutside(false);
        return dialog;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        assert which == AlertDialog.BUTTON_POSITIVE;

        // Remember that the dialog about other forms of browsing history has been shown
        // to the user.
        recordDialogWasShown(getActivity(), true);

        // Finishes the ClearBrowsingDataPreferences activity that created this dialog.
        getActivity().finish();
    }

    /**
     * Sets the preference indicating whether this dialog was already shown.
     * @param activity The Activity storing the preference.
     * @param shown Whether the dialog was shown.
     */
    private static void recordDialogWasShown(Activity activity, boolean shown) {
        SharedPreferences preferences =
                ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean(PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN, shown);
        editor.apply();
    }

    /**
     * @return Whether the dialog has already been shown to the user before.
     */
    static boolean wasDialogShown() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN, false);
    }

    /**
     * For testing purposes, resets the preference indicating that this dialog has been shown
     * to false.
     * @param activity The Activity storing the preference.
     */
    @VisibleForTesting
    static void clearShownPreferenceForTesting(Activity activity) {
        recordDialogWasShown(activity, false);
    }
}
