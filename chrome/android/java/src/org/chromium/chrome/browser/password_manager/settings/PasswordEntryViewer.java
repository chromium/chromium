// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.SETTINGS_REAUTHENTICATION_HISTOGRAM;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.fragment.app.Fragment;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.password_manager.ReauthResult;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

/**
 * Password entry viewer that allows to view and delete passwords stored in Chrome.
 */
public class PasswordEntryViewer
        extends Fragment implements PasswordManagerHandler.PasswordListObserver {
    // Constants used to log UMA enum histogram, must stay in sync with
    // PasswordManagerAndroidPasswordEntryActions. Further actions can only be appended, existing
    // entries must not be overwritten.
    private static final int PASSWORD_ENTRY_ACTION_VIEWED = 0;
    private static final int PASSWORD_ENTRY_ACTION_DELETED = 1;
    // Value 2 used to mean 'cancel' and is now obsolete. See https://crbug.com/807577 for details.
    private static final int PASSWORD_ENTRY_ACTION_VIEWED_AFTER_SEARCH = 3;
    private static final int PASSWORD_ENTRY_ACTION_BOUNDARY = 4;

    // Constants used to log UMA enum histogram, must stay in sync with
    // PasswordManagerAndroidWebsiteActions. Further actions can only be appended, existing
    // entries must not be overwritten.
    // NOTE: BOUNDARY must be at least 2, even if only COPIED currently exists (crbug.com/781959).
    private static final int WEBSITE_ACTION_COPIED = 0;
    private static final int WEBSITE_ACTION_BOUNDARY = 2;

    // Constants used to log UMA enum histogram, must stay in sync with
    // PasswordManagerAndroidUsernameActions. Further actions can only be appended, existing
    // entries must not be overwritten.
    // NOTE: BOUNDARY must be at least 2, even if only COPIED currently exists (crbug.com/781959).
    private static final int USERNAME_ACTION_COPIED = 0;
    private static final int USERNAME_ACTION_BOUNDARY = 2;

    // Constants used to log UMA enum histogram, must stay in sync with
    // PasswordManagerAndroidPasswordActions. Further actions can only be appended, existing
    // entries must not be overwritten.
    private static final int PASSWORD_ACTION_COPIED = 0;
    private static final int PASSWORD_ACTION_DISPLAYED = 1;
    private static final int PASSWORD_ACTION_HIDDEN = 2;
    private static final int PASSWORD_ACTION_BOUNDARY = 3;

    // Metrics: "PasswordManager.AccessPasswordInSettings"
    private static final int ACCESS_PASSWORD_VIEWED = 0;
    private static final int ACCESS_PASSWORD_COPIED = 1;
    private static final int ACCESS_PASSWORD_COUNT = 2;

    // ID of this name/password or exception.
    private int mID;

    // If true this is an exception site (never save here).
    // If false this represents a saved name/password.
    private boolean mException;

    private ClipboardManager mClipboard;
    private Bundle mExtras;
    private View mView;
    private boolean mViewButtonPressed;
    private boolean mCopyButtonPressed;
    private boolean mFoundViaSearch;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Extras are set on this intent in class {@link PasswordSettings}.
        mExtras = getArguments();
        assert mExtras != null;
        mID = mExtras.getInt(PasswordSettings.PASSWORD_LIST_ID);
        mFoundViaSearch = mExtras.getBoolean(PasswordSettings.EXTRA_FOUND_VIA_SEARCH, false);
        final String name = mExtras.containsKey(PasswordSettings.PASSWORD_LIST_NAME)
                ? mExtras.getString(PasswordSettings.PASSWORD_LIST_NAME)
                : null;

        mException = (name == null);
        final String url = mExtras.getString(PasswordSettings.PASSWORD_LIST_URL);
        mClipboard = (ClipboardManager) getActivity().getApplicationContext().getSystemService(
                Context.CLIPBOARD_SERVICE);
        View inflatedView =
                inflater.inflate(mException ? R.layout.password_entry_exception
                                            : R.layout.password_entry_viewer_interactive,
                        container, false);
        mView = inflatedView.findViewById(R.id.scroll_view);
        getActivity().setTitle(R.string.password_entry_viewer_title);
        mClipboard = (ClipboardManager) getActivity().getApplicationContext().getSystemService(
                Context.CLIPBOARD_SERVICE);
        setRowText(R.id.url_row, url);
        mView.getViewTreeObserver().addOnScrollChangedListener(
                SettingsUtils.getShowShadowOnScrollListener(
                        mView, inflatedView.findViewById(R.id.shadow)));

        hookupCopySiteButton(mView.findViewById(R.id.url_row));
        if (!mException) {
            getActivity().setTitle(R.string.password_entry_viewer_title);
            setRowText(R.id.username_row, name);
            hookupCopyUsernameButton(mView.findViewById(R.id.username_row));
            if (ReauthenticationManager.isReauthenticationApiAvailable()) {
                hidePassword();
                hookupPasswordButtons();
            } else {
                mView.findViewById(R.id.password_data).setVisibility(View.GONE);
                if (isPasswordSyncingUser()) {
                    ForegroundColorSpan colorSpan =
                            new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                    getResources(), R.color.default_control_color_active));
                    SpannableString passwordLink =
                            SpanApplier.applySpans(getString(R.string.manage_passwords_text),
                                    new SpanApplier.SpanInfo("<link>", "</link>", colorSpan));
                    ClickableSpan clickableLink = new ClickableSpan() {
                        @Override
                        public void onClick(View textView) {
                            Intent intent = new Intent(Intent.ACTION_VIEW,
                                    Uri.parse(PasswordUIView.getAccountDashboardURL()));
                            intent.setPackage(getActivity().getPackageName());
                            getActivity().startActivity(intent);
                        }

                        @Override
                        public void updateDrawState(TextPaint ds) {}
                    };
                    passwordLink.setSpan(clickableLink, 0, passwordLink.length(),
                            Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
                    TextView passwordsLinkTextView = mView.findViewById(R.id.passwords_link);
                    passwordsLinkTextView.setVisibility(View.VISIBLE);
                    passwordsLinkTextView.setText(passwordLink);
                    passwordsLinkTextView.setMovementMethod(LinkMovementMethod.getInstance());
                } else {
                    mView.findViewById(R.id.password_title).setVisibility(View.GONE);
                }
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordCredentialEntry", PASSWORD_ENTRY_ACTION_VIEWED,
                    PASSWORD_ENTRY_ACTION_BOUNDARY);
            // Additionally, save whether the entry was found via the Preference's search function.
            if (mFoundViaSearch) {
                RecordHistogram.recordEnumeratedHistogram(
                        "PasswordManager.Android.PasswordCredentialEntry",
                        PASSWORD_ENTRY_ACTION_VIEWED_AFTER_SEARCH, PASSWORD_ENTRY_ACTION_BOUNDARY);
            }

        } else {
            getActivity().setTitle(R.string.section_saved_passwords_exceptions);
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordExceptionEntry", PASSWORD_ENTRY_ACTION_VIEWED,
                    PASSWORD_ENTRY_ACTION_BOUNDARY);
        }

        return inflatedView;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (ReauthenticationManager.authenticationStillValid(
                    ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
            if (mViewButtonPressed) displayPassword();

            if (mCopyButtonPressed) copyPassword();
        }
        PasswordManagerHandlerProvider.getInstance().addObserver(this);
        PasswordManagerHandlerProvider.getInstance()
                .getPasswordManagerHandler()
                .updatePasswordLists();
    }

    @Override
    public void onPause() {
        super.onPause();
        PasswordManagerHandlerProvider.getInstance().removeObserver(this);
    }

    private boolean isPasswordSyncingUser() {
        ProfileSyncService syncService = ProfileSyncService.get();
        return syncService != null && syncService.isSyncRequested()
                && syncService.isEngineInitialized() && !syncService.isUsingSecondaryPassphrase();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.password_entry_viewer_action_bar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_delete_saved_password) {
            removeItem();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // Delete was clicked.
    private void removeItem() {
        final PasswordManagerHandler.PasswordListObserver passwordDeleter =
                new PasswordManagerHandler.PasswordListObserver() {
                    @Override
                    public void passwordListAvailable(int count) {
                        if (!mException) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "PasswordManager.Android.PasswordCredentialEntry",
                                    PASSWORD_ENTRY_ACTION_DELETED, PASSWORD_ENTRY_ACTION_BOUNDARY);
                            PasswordManagerHandlerProvider.getInstance()
                                    .getPasswordManagerHandler()
                                    .removeSavedPasswordEntry(mID);
                            PasswordManagerHandlerProvider.getInstance().removeObserver(this);
                            Toast.makeText(getActivity().getApplicationContext(), R.string.deleted,
                                         Toast.LENGTH_SHORT)
                                    .show();
                            getActivity().finish();
                        }
                    }

                    @Override
                    public void passwordExceptionListAvailable(int count) {
                        if (mException) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "PasswordManager.Android.PasswordExceptionEntry",
                                    PASSWORD_ENTRY_ACTION_DELETED, PASSWORD_ENTRY_ACTION_BOUNDARY);
                            PasswordManagerHandlerProvider.getInstance()
                                    .getPasswordManagerHandler()
                                    .removeSavedPasswordException(mID);
                            PasswordManagerHandlerProvider.getInstance().removeObserver(this);
                            Toast.makeText(getActivity().getApplicationContext(), R.string.deleted,
                                         Toast.LENGTH_SHORT)
                                    .show();
                            getActivity().finish();
                        }
                    }
                };

        PasswordManagerHandlerProvider.getInstance().addObserver(passwordDeleter);
        PasswordManagerHandlerProvider.getInstance()
                .getPasswordManagerHandler()
                .updatePasswordLists();
    }

    private void hookupCopyUsernameButton(View usernameView) {
        final ImageButton copyUsernameButton =
                usernameView.findViewById(R.id.password_entry_viewer_copy);
        copyUsernameButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));

        copyUsernameButton.setContentDescription(
                getActivity().getString(R.string.password_entry_viewer_copy_stored_username));
        copyUsernameButton.setOnClickListener(v -> {
            ClipData clip = ClipData.newPlainText(
                    "username", getArguments().getString(PasswordSettings.PASSWORD_LIST_NAME));
            mClipboard.setPrimaryClip(clip);
            Toast.makeText(getActivity().getApplicationContext(),
                         R.string.password_entry_viewer_username_copied_into_clipboard,
                         Toast.LENGTH_SHORT)
                    .show();
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordCredentialEntry.Username",
                    USERNAME_ACTION_COPIED, USERNAME_ACTION_BOUNDARY);
        });
    }

    private void hookupCopySiteButton(View siteView) {
        final ImageButton copySiteButton = siteView.findViewById(R.id.password_entry_viewer_copy);
        copySiteButton.setContentDescription(
                getActivity().getString(R.string.password_entry_viewer_copy_stored_site));
        copySiteButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));

        copySiteButton.setOnClickListener(v -> {
            ClipData clip = ClipData.newPlainText(
                    "site", getArguments().getString(PasswordSettings.PASSWORD_LIST_URL));
            mClipboard.setPrimaryClip(clip);
            Toast.makeText(getActivity().getApplicationContext(),
                         R.string.password_entry_viewer_site_copied_into_clipboard,
                         Toast.LENGTH_SHORT)
                    .show();
            if (mException) {
                RecordHistogram.recordEnumeratedHistogram(
                        "PasswordManager.Android.PasswordExceptionEntry.Website",
                        WEBSITE_ACTION_COPIED, WEBSITE_ACTION_BOUNDARY);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        "PasswordManager.Android.PasswordCredentialEntry.Website",
                        WEBSITE_ACTION_COPIED, WEBSITE_ACTION_BOUNDARY);
            }
        });
    }

    private void changeHowPasswordIsDisplayed(
            int visibilityIcon, int inputType, @StringRes int annotation) {
        TextView passwordView = mView.findViewById(R.id.password_entry_viewer_password);
        ImageButton viewPasswordButton =
                mView.findViewById(R.id.password_entry_viewer_view_password);
        passwordView.setText(mExtras.getString(PasswordSettings.PASSWORD_LIST_PASSWORD));
        passwordView.setInputType(inputType);
        viewPasswordButton.setImageResource(visibilityIcon);
        viewPasswordButton.setContentDescription(getActivity().getString(annotation));
    }

    private void displayPassword() {
        getActivity().getWindow().setFlags(LayoutParams.FLAG_SECURE, LayoutParams.FLAG_SECURE);

        changeHowPasswordIsDisplayed(R.drawable.ic_visibility_off_black,
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE,
                R.string.password_entry_viewer_hide_stored_password);
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password",
                PASSWORD_ACTION_DISPLAYED, PASSWORD_ACTION_BOUNDARY);

        RecordHistogram.recordEnumeratedHistogram("PasswordManager.AccessPasswordInSettings",
                ACCESS_PASSWORD_VIEWED, ACCESS_PASSWORD_COUNT);
    }

    private void hidePassword() {
        getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);

        changeHowPasswordIsDisplayed(R.drawable.ic_visibility_black,
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE,
                R.string.password_entry_viewer_view_stored_password);
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password", PASSWORD_ACTION_HIDDEN,
                PASSWORD_ACTION_BOUNDARY);
    }

    private void copyPassword() {
        ClipData clip = ClipData.newPlainText(
                "password", getArguments().getString(PasswordSettings.PASSWORD_LIST_PASSWORD));
        mClipboard.setPrimaryClip(clip);
        Toast.makeText(getActivity().getApplicationContext(),
                     R.string.password_entry_viewer_password_copied_into_clipboard,
                     Toast.LENGTH_SHORT)
                .show();
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password", PASSWORD_ACTION_COPIED,
                PASSWORD_ACTION_BOUNDARY);

        RecordHistogram.recordEnumeratedHistogram("PasswordManager.AccessPasswordInSettings",
                ACCESS_PASSWORD_COPIED, ACCESS_PASSWORD_COUNT);
    }

    private void hookupPasswordButtons() {
        final ImageButton copyPasswordButton =
                mView.findViewById(R.id.password_entry_viewer_copy_password);
        final ImageButton viewPasswordButton =
                mView.findViewById(R.id.password_entry_viewer_view_password);
        copyPasswordButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));
        copyPasswordButton.setOnClickListener(v -> {
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_entry_viewer_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
            } else if (ReauthenticationManager.authenticationStillValid(
                               ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                RecordHistogram.recordEnumeratedHistogram(SETTINGS_REAUTHENTICATION_HISTOGRAM,
                        ReauthResult.SKIPPED, ReauthResult.MAX_VALUE);
                copyPassword();
            } else {
                mCopyButtonPressed = true;
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_copy,
                        R.id.password_entry_viewer_interactive, getFragmentManager(),
                        ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
            }
        });
        viewPasswordButton.setOnClickListener(v -> {
            TextView passwordView = mView.findViewById(R.id.password_entry_viewer_password);
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_entry_viewer_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
            } else if ((passwordView.getInputType()
                               & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                    == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD) {
                hidePassword();
            } else if (ReauthenticationManager.authenticationStillValid(
                               ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                RecordHistogram.recordEnumeratedHistogram(SETTINGS_REAUTHENTICATION_HISTOGRAM,
                        ReauthResult.SKIPPED, ReauthResult.MAX_VALUE);
                displayPassword();
            } else {
                mViewButtonPressed = true;
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_view,
                        R.id.password_entry_viewer_interactive, getFragmentManager(),
                        ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
            }
        });
    }

    private void setRowText(@IdRes int rowId, String text) {
        View rowView = mView.findViewById(rowId);
        TextView dataView = rowView.findViewById(R.id.password_entry_viewer_row_data);
        dataView.setText(text);
    }

    @Override
    public void passwordListAvailable(int count) {
        if (mException) return;
        TextView passwordTextView = mView.findViewById(R.id.password_entry_viewer_password);
        SavedPasswordEntry savedPasswordEntry = PasswordManagerHandlerProvider.getInstance()
                                                        .getPasswordManagerHandler()
                                                        .getSavedPasswordEntry(mID);
        setRowText(R.id.url_row, savedPasswordEntry.getUrl());
        setRowText(R.id.username_row, savedPasswordEntry.getUserName());
        passwordTextView.setText(savedPasswordEntry.getPassword());
    }

    @Override
    public void passwordExceptionListAvailable(int count) {}
}
