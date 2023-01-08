package com.ark.browser.ui.fragment.collection.password;

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
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.KeyguardUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.settings.PasswordUIView;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.SavedPasswordEntry;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.text.SpanApplier;
import org.chromium.chrome.R;

/**
 * Password entry editor that allows to view and delete passwords stored in Chrome.
 */
public class PasswordEntryEditor extends BaseSwipeBackFragment {

    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Constants used to log UMA enum histogram, must stay in sync with
    // PasswordManagerAndroidPasswordEntryActions. Further actions can only be appended, existing
    // entries must not be overwritten.
    private static final int PASSWORD_ENTRY_ACTION_VIEWED = 0;
    private static final int PASSWORD_ENTRY_ACTION_DELETED = 1;
    private static final int PASSWORD_ENTRY_ACTION_CANCELLED = 2;
    private static final int PASSWORD_ENTRY_ACTION_BOUNDARY = 3;

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

    // If true this is an exception site (never save here).
    // If false this represents a saved name/password.
    private String mName;
    private String mUrl;
    private boolean mException;

    private Bundle mExtras;
    private View mView;
    private boolean mViewButtonPressed;
    private boolean mCopyButtonPressed;

    private Runnable mRemoveRunnable;


    public static PasswordEntryEditor newInstance(SavedPasswordEntry entry) {
        Bundle args = new Bundle();
        args.putString(PASSWORD_LIST_NAME, entry.getUserName());
        args.putString(PASSWORD_LIST_URL, entry.getUrl());
        args.putString(PASSWORD_LIST_PASSWORD, entry.getPassword());
        PasswordEntryEditor fragment = new PasswordEntryEditor();
        fragment.setArguments(args);
        return fragment;
    }

    public void setRemoveRunnable(Runnable runnable) {
        mRemoveRunnable = runnable;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
        if (mRemoveRunnable == null) {
            popThis();
        }
    }

    @Override
    protected int getLayoutId() {
        mExtras = getArguments();
        assert mExtras != null;
        mName= mExtras.containsKey(PASSWORD_LIST_NAME)
                ? mExtras.getString(PASSWORD_LIST_NAME)
                : null;

        mException = (mName == null);
        mUrl = mExtras.getString(PASSWORD_LIST_URL);
        return mException ? R.layout.password_entry_exception
                : R.layout.password_entry_editor_interactive;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        if (mRemoveRunnable == null) {
            popThis();
            return;
        }
        mView = view;
        View urlRowsView = view.findViewById(R.id.url_row);
        TextView dataView = urlRowsView.findViewById(R.id.password_entry_editor_row_data);
        dataView.setText(mUrl);

        hookupCopySiteButton(urlRowsView);
        if (!mException) {
            View usernameView = view.findViewById(R.id.username_row);
            TextView usernameDataView =
                    usernameView.findViewById(R.id.password_entry_editor_row_data);
            usernameDataView.setText(mName);
            hookupCopyUsernameButton(usernameView);
            if (ReauthenticationManager.isReauthenticationApiAvailable()) {
                hidePassword();
                hookupPasswordButtons();
            } else {
                view.findViewById(R.id.password_data).setVisibility(View.GONE);
                if (isPasswordSyncingUser()) {
                    ForegroundColorSpan colorSpan =
                            new ForegroundColorSpan(SkinEngine.getColor(context, R.attr.colorPrimary));
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
                    TextView passwordsLinkTextView = view.findViewById(R.id.passwords_link);
                    passwordsLinkTextView.setVisibility(View.VISIBLE);
                    passwordsLinkTextView.setText(passwordLink);
                    passwordsLinkTextView.setMovementMethod(LinkMovementMethod.getInstance());
                } else {
                    view.findViewById(R.id.password_title).setVisibility(View.GONE);
                }
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordCredentialEntry", PASSWORD_ENTRY_ACTION_VIEWED,
                    PASSWORD_ENTRY_ACTION_BOUNDARY);

        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordExceptionEntry", PASSWORD_ENTRY_ACTION_VIEWED,
                    PASSWORD_ENTRY_ACTION_BOUNDARY);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (ReauthenticationManager.authenticationStillValid()) {
            if (mViewButtonPressed) displayPassword();

            if (mCopyButtonPressed) copyPassword();
        }
    }

    private boolean isPasswordSyncingUser() {
        return true;
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

    // TODO Delete was clicked.
    private void removeItem() {
        ZDialog.alert()
                .setTitle("确定删除?")
                .setContent("你将删除" + mUrl + "保存的密码")
                .setPositiveButton((fragment1, which) -> {
                    mRemoveRunnable.run();
                    popThis();
                })
                .show(context);
    }

    private void hookupCopyUsernameButton(View usernameView) {
        final ImageButton copyUsernameButton =
                usernameView.findViewById(R.id.password_entry_editor_copy);
        copyUsernameButton.setImageResource(R.drawable.ic_content_copy_black);

        copyUsernameButton.setContentDescription(
                getString(R.string.password_entry_editor_copy_stored_username));
        copyUsernameButton.setOnClickListener(v -> {
//            Clipboard.getInstance().setText("username", getArguments().getString(PASSWORD_LIST_NAME));
            Clipboard.getInstance().setTextFromUser(getArguments().getString(PASSWORD_LIST_NAME));
            ZToast.success(R.string.password_entry_editor_username_copied_into_clipboard);
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.Android.PasswordCredentialEntry.Username",
                    USERNAME_ACTION_COPIED, USERNAME_ACTION_BOUNDARY);
        });
    }

    private void hookupCopySiteButton(View siteView) {
        final ImageButton copySiteButton =
                siteView.findViewById(R.id.password_entry_editor_copy);
        copySiteButton.setContentDescription(
                getString(R.string.password_entry_editor_copy_stored_site));
        copySiteButton.setImageResource(R.drawable.ic_content_copy_black);

        copySiteButton.setOnClickListener(v -> {
//            Clipboard.getInstance().setText("site", getArguments().getString(PASSWORD_LIST_URL));
            Clipboard.getInstance().setTextFromUser(getArguments().getString(PASSWORD_LIST_URL));
            ZToast.success(R.string.password_entry_editor_site_copied_into_clipboard);
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
        passwordView.setText(mExtras.getString(PASSWORD_LIST_PASSWORD));
        passwordView.setInputType(inputType);
        viewPasswordButton.setImageResource(visibilityIcon);
        viewPasswordButton.setContentDescription(getString(annotation));
    }

    private void displayPassword() {
        _mActivity.getWindow().setFlags(LayoutParams.FLAG_SECURE, LayoutParams.FLAG_SECURE);

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
        Clipboard.getInstance().setTextFromUser(getArguments().getString(PASSWORD_LIST_PASSWORD));
        ZToast.success(R.string.password_entry_editor_password_copied_into_clipboard);
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.Android.PasswordCredentialEntry.Password", PASSWORD_ACTION_COPIED,
                PASSWORD_ACTION_BOUNDARY);
    }

    private void hookupPasswordButtons() {
        final ImageButton copyPasswordButton =
                mView.findViewById(R.id.password_entry_editor_copy_password);
        final ImageButton viewPasswordButton =
                mView.findViewById(R.id.password_entry_editor_view_password);
        copyPasswordButton.setImageResource(R.drawable.ic_content_copy_black);
        copyPasswordButton.setOnClickListener(v -> {
            if (!ReauthenticationManager.isScreenLockSetUp(context)) {
                ZToast.warning(R.string.password_entry_editor_set_lock_screen);
            } else if (ReauthenticationManager.authenticationStillValid()) {
                copyPassword();
            } else {
                mCopyButtonPressed = true;
                KeyguardUtil.with(getActivity()).lockDevice();
//                start(PasswordReauthenticationFragment.newInstance());
//                ReauthenticationManager.displayReauthenticationFragment(
//                        R.string.lockscreen_description_copy,
//                        R.id.password_entry_editor_interactive, getFragmentManager());
            }
        });
        viewPasswordButton.setOnClickListener(v -> {
            TextView passwordView = mView.findViewById(R.id.password_entry_editor_password);
            if (!ReauthenticationManager.isScreenLockSetUp(context)) {
                ZToast.warning(R.string.password_entry_editor_set_lock_screen);
            } else if ((passwordView.getInputType()
                    & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                    == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD) {
                hidePassword();
            } else if (ReauthenticationManager.authenticationStillValid()) {
                displayPassword();
            } else {
                mViewButtonPressed = true;
                KeyguardUtil.with(getActivity()).lockDevice();
//                start(PasswordReauthenticationFragment.newInstance());
//                ReauthenticationManager.displayReauthenticationFragment(
//                        R.string.lockscreen_description_view,
//                        R.id.password_entry_editor_interactive, getFragmentManager());
            }
        });
    }
}

