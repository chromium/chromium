// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.app.Fragment;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.StringRes;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
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
import android.view.WindowManager.LayoutParams;
import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

/**
 * Password entry editor that allows to view and delete passwords stored in Chrome.
 */
public class PasswordEntryEditor extends Fragment {
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
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Extras are set on this intent in class {@link SavePasswordsPreferences}.
        mExtras = getArguments();
        assert mExtras != null;
        mID = mExtras.getInt(SavePasswordsPreferences.PASSWORD_LIST_ID);
        mFoundViaSearch = getActivity().getIntent().getBooleanExtra(
                SavePasswordsPreferences.EXTRA_FOUND_VIA_SEARCH, false);
        final String name = mExtras.containsKey(SavePasswordsPreferences.PASSWORD_LIST_NAME)
                ? mExtras.getString(SavePasswordsPreferences.PASSWORD_LIST_NAME)
                : null;

        mException = (name == null);
        final String url = mExtras.getString(SavePasswordsPreferences.PASSWORD_LIST_URL);
        getActivity().setTitle(R.string.password_entry_editor_title);
        mClipboard = (ClipboardManager) getActivity().getApplicationContext().getSystemService(
                Context.CLIPBOARD_SERVICE);
        View inflatedView =
                inflater.inflate(mException ? R.layout.password_entry_exception
                                            : R.layout.password_entry_editor_interactive,
                        container, false);
        mView = inflatedView.findViewById(R.id.scroll_view);
        getActivity().setTitle(R.string.password_entry_editor_title);
        mClipboard = (ClipboardManager) getActivity().getApplicationContext().getSystemService(
                Context.CLIPBOARD_SERVICE);
        View urlRowsView = mView.findViewById(R.id.url_row);
        TextView dataView = urlRowsView.findViewById(R.id.password_entry_editor_row_data);
        dataView.setText(url);
        mView.getViewTreeObserver().addOnScrollChangedListener(
                PreferenceUtils.getShowShadowOnScrollListener(
                        mView, inflatedView.findViewById(R.id.shadow)));

        hookupCopySiteButton(urlRowsView);
        if (!mException) {
            View usernameView = mView.findViewById(R.id.username_row);
            TextView usernameDataView =
                    usernameView.findViewById(R.id.password_entry_editor_row_data);
            usernameDataView.setText(name);
            hookupCopyUsernameButton(usernameView);
            if (ReauthenticationManager.isReauthenticationApiAvailable()) {
                hidePassword();
                hookupPasswordButtons();
            } else {
                mView.findViewById(R.id.password_data).setVisibility(View.GONE);
                if (isPasswordSyncingUser()) {
                    ForegroundColorSpan colorSpan =
                            new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                    getResources(), R.color.pref_accent_color));
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
    }

    private boolean isPasswordSyncingUser() {
        ProfileSyncService syncService = ProfileSyncService.get();
        return (AndroidSyncSettings.isSyncEnabled() && syncService.isEngineInitialized()
                && !syncService.isUsingSecondaryPassphrase());
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.password_entry_editor_action_bar_menu, menu);
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
        final PasswordManagerHandler.PasswordListObserver
                passwordDeleter = new PasswordManagerHandler.PasswordListObserver() {
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
        final AppCompatImageButton copyUsernameButton =
                usernameView.findViewById(R.id.password_entry_editor_copy);
        copyUsernameButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));

        copyUsernameButton.setContentDescription(
                getActivity().getString(R.string.password_entry_editor_copy_stored_username));
        copyUsernameButton.setOnClickListener(v -> {
            ClipData clip = ClipData.newPlainText("username",
                    getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_NAME));
            mClipboard.setPrimaryClip(clip);
            Toast.makeText(getActivity().getApplicationContext(),
                         R.string.password_entry_editor_username_copied_into_clipboard,
                         Toast.LENGTH_SHORT)
                    .show();
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordCredentialEntry.Username",
                    USERNAME_ACTION_COPIED, USERNAME_ACTION_BOUNDARY);
        });
    }

    private void hookupCopySiteButton(View siteView) {
        final AppCompatImageButton copySiteButton =
                siteView.findViewById(R.id.password_entry_editor_copy);
        copySiteButton.setContentDescription(
                getActivity().getString(R.string.password_entry_editor_copy_stored_site));
        copySiteButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));

        copySiteButton.setOnClickListener(v -> {
            ClipData clip = ClipData.newPlainText(
                    "site", getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_URL));
            mClipboard.setPrimaryClip(clip);
            Toast.makeText(getActivity().getApplicationContext(),
                         R.string.password_entry_editor_site_copied_into_clipboard,
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
        TextView passwordView = mView.findViewById(R.id.password_entry_editor_password);
        ImageButton viewPasswordButton =
                mView.findViewById(R.id.password_entry_editor_view_password);
        passwordView.setText(mExtras.getString(SavePasswordsPreferences.PASSWORD_LIST_PASSWORD));
        passwordView.setInputType(inputType);
        viewPasswordButton.setImageResource(visibilityIcon);
        viewPasswordButton.setContentDescription(getActivity().getString(annotation));
    }

    private void displayPassword() {
        getActivity().getWindow().setFlags(LayoutParams.FLAG_SECURE, LayoutParams.FLAG_SECURE);

        changeHowPasswordIsDisplayed(R.drawable.ic_visibility_off_black,
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE,
                R.string.password_entry_editor_hide_stored_password);
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password",
                PASSWORD_ACTION_DISPLAYED, PASSWORD_ACTION_BOUNDARY);
    }

    private void hidePassword() {
        changeHowPasswordIsDisplayed(R.drawable.ic_visibility_black,
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE,
                R.string.password_entry_editor_view_stored_password);
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password", PASSWORD_ACTION_HIDDEN,
                PASSWORD_ACTION_BOUNDARY);
    }

    private void copyPassword() {
        ClipData clip = ClipData.newPlainText("password",
                getArguments().getString(SavePasswordsPreferences.PASSWORD_LIST_PASSWORD));
        mClipboard.setPrimaryClip(clip);
        Toast.makeText(getActivity().getApplicationContext(),
                     R.string.password_entry_editor_password_copied_into_clipboard,
                     Toast.LENGTH_SHORT)
                .show();
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password", PASSWORD_ACTION_COPIED,
                PASSWORD_ACTION_BOUNDARY);
    }

    private void hookupPasswordButtons() {
        final AppCompatImageButton copyPasswordButton =
                mView.findViewById(R.id.password_entry_editor_copy_password);
        final ImageButton viewPasswordButton =
                mView.findViewById(R.id.password_entry_editor_view_password);
        copyPasswordButton.setImageDrawable(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_content_copy_black));
        copyPasswordButton.setOnClickListener(v -> {
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_entry_editor_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
            } else if (ReauthenticationManager.authenticationStillValid(
                               ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                copyPassword();
            } else {
                mCopyButtonPressed = true;
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_copy,
                        R.id.password_entry_editor_interactive, getFragmentManager(),
                        ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
            }
        });
        viewPasswordButton.setOnClickListener(v -> {
            TextView passwordView = mView.findViewById(R.id.password_entry_editor_password);
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                Toast.makeText(getActivity().getApplicationContext(),
                             R.string.password_entry_editor_set_lock_screen, Toast.LENGTH_LONG)
                        .show();
            } else if ((passwordView.getInputType()
                               & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                    == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD) {
                hidePassword();
            } else if (ReauthenticationManager.authenticationStillValid(
                               ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                displayPassword();
            } else {
                mViewButtonPressed = true;
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_view,
                        R.id.password_entry_editor_interactive, getFragmentManager(),
                        ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
            }
        });
    }
}
