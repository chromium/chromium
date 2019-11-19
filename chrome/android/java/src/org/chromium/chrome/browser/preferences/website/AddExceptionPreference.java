// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Vibrator;
import android.provider.Settings;
import android.support.v7.app.AlertDialog;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * A utility class for the UI recording exceptions to the blocked list for site
 * settings.
 */
public class AddExceptionPreference
        extends Preference implements Preference.OnPreferenceClickListener {
    // The callback to notify when the user adds a site.
    private SiteAddedCallback mSiteAddedCallback;

    // The accent color to use for the icon and title view.
    private int mPrefAccentColor;

    // The custom message to show in the dialog.
    private String mDialogMessage;

    // The colors for the site URL EditText
    private int mErrorColor;
    private int mDefaultColor;

    /**
     * An interface to implement to get a callback when a site needs to be added.
     * @param hostname The hostname to add.
     */
    public interface SiteAddedCallback {
        public void onAddSite(String hostname);
    }

    /**
     * Construct a AddException preference.
     * @param context The current context.
     * @param key The key to use for the preference.
     * @param message The custom message to show in the dialog.
     * @param callback A callback to receive notifications that an exception has been added.
     */
    public AddExceptionPreference(
            Context context, String key, String message, SiteAddedCallback callback) {
        super(context);
        mDialogMessage = message;
        mSiteAddedCallback = callback;
        setOnPreferenceClickListener(this);

        setKey(key);
        Resources resources = getContext().getResources();
        mPrefAccentColor = ApiCompatibilityUtils.getColor(resources, R.color.pref_accent_color);
        mErrorColor = resources.getColor(R.color.default_red);
        mDefaultColor = resources.getColor(R.color.default_text_color);

        Drawable plusIcon = ApiCompatibilityUtils.getDrawable(resources, R.drawable.plus);
        plusIcon.mutate();
        plusIcon.setColorFilter(mPrefAccentColor, PorterDuff.Mode.SRC_IN);
        setIcon(plusIcon);

        setTitle(resources.getString(R.string.website_settings_add_site));
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setAllCaps(true);
        titleView.setTextColor(mPrefAccentColor);
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        showAddExceptionDialog();
        return true;
    }

    /**
     * Show the dialog allowing the user to add a new website as an exception.
     */
    private void showAddExceptionDialog() {
        LayoutInflater inflater = (LayoutInflater) getContext().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.add_site_dialog, null);
        final EditText input = (EditText) view.findViewById(R.id.site);

        DialogInterface.OnClickListener onClickListener = new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int button) {
                if (button == AlertDialog.BUTTON_POSITIVE) {
                    mSiteAddedCallback.onAddSite(input.getText().toString().trim());
                } else {
                    dialog.dismiss();
                }
            }
        };

        AlertDialog.Builder alert =
                new AlertDialog.Builder(getContext(), R.style.Theme_Chromium_AlertDialog);
        AlertDialog alertDialog = alert
                .setTitle(R.string.website_settings_add_site_dialog_title)
                .setMessage(mDialogMessage)
                .setView(view)
                .setPositiveButton(R.string.website_settings_add_site_add_button, onClickListener)
                .setNegativeButton(R.string.cancel, onClickListener)
                .create();
        alertDialog.getDelegate().setHandleNativeActionModesEnabled(false);
        alertDialog.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow(DialogInterface dialog) {
                KeyboardVisibilityDelegate.getInstance().showKeyboard(input);
            }
        });
        alertDialog.show();
        final Button okButton = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        okButton.setEnabled(false);

        input.addTextChangedListener(new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {}

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                // The intent is to capture a hostname and register it as an exception using a
                // pattern. But a pattern can be used to express things that are not supported, such
                // as domains, schemes and ports. Therefore we need to filter out invalid values
                // before passing them on to the validity checker for patterns.
                String hostname = s.toString().trim();
                boolean hasError = hostname.length() > 0
                        && (hostname.contains(":") || hostname.contains(" ")
                                || hostname.startsWith(".")
                                || !WebsitePreferenceBridgeJni.get().isContentSettingsPatternValid(
                                        hostname));

                // Vibrate when adding characters only, not when deleting them.
                if (hasError && count != 0) {
                    if (Settings.System.getInt(getContext().getContentResolver(),
                            Settings.System.HAPTIC_FEEDBACK_ENABLED, 1) == 1) {
                        ((Vibrator) getContext().getSystemService(
                                Context.VIBRATOR_SERVICE)).vibrate(50);
                    }
                }

                okButton.setEnabled(!hasError && hostname.length() > 0);
                input.setTextColor(hasError ? mErrorColor : mDefaultColor);
            }
        });
    }
}
