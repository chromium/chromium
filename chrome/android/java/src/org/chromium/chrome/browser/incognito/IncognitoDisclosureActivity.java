// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.CheckBox;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogView.ButtonType;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Activity that shows a dialog with a warning before opening an incognito Custom Tab when
 * incognito tabs are already present.
 * If user chooses to proceed, opens the custom tab.
 */
public class IncognitoDisclosureActivity extends AppCompatActivity {
    private static final String EXTRA_CUSTOM_TAB_INTENT = "extra_custom_tab_intent";

    /** Launches the activity */
    public static void launch(Context context, Intent customTabIntent) {
        Intent intent = new Intent(context, IncognitoDisclosureActivity.class);
        intent.putExtra(EXTRA_CUSTOM_TAB_INTENT, customTabIntent);
        context.startActivity(intent);
    }

    private boolean mCloseIncognitoTabs;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View contentView =
                getLayoutInflater().inflate(R.layout.incognito_disclosure_dialog_content, null);
        CheckBox checkBox =
                contentView.findViewById(R.id.incognito_disclosure_close_incognito_checkbox);
        checkBox.setOnCheckedChangeListener((view, isChecked) -> mCloseIncognitoTabs = isChecked);

        ModalDialogView.Params params = new ModalDialogView.Params();
        params.customView = contentView;
        params.title = getString(R.string.incognito_disclosure_title);
        params.positiveButtonText = getString(R.string.ok_got_it);
        params.negativeButtonText = getString(R.string.cancel);

        ModalDialogView dialog = new ModalDialogView(mDialogController, params);

        new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP)
                .showDialog(dialog, ModalDialogType.APP);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mOpenCustomTabAfterCleanUpTask.cancel(true);
    }

    private void openCustomTabActivity() {
        startActivity(getIntent().getParcelableExtra(EXTRA_CUSTOM_TAB_INTENT));
        finish();
    }

    private final ModalDialogView.Controller mDialogController = new ModalDialogView.Controller() {
        @Override
        public void onClick(@ButtonType int buttonType) {
            if (buttonType == ButtonType.NEGATIVE) {
                finish();
                return;
            }
            if (mCloseIncognitoTabs) {
                mOpenCustomTabAfterCleanUpTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            } else {
                openCustomTabActivity();
            }
        }

        @Override
        public void onDismiss(@DialogDismissalCause int dismissalCause) {
            finish();
        }
    };

    private AsyncTask<Void> mOpenCustomTabAfterCleanUpTask = new AsyncTask<Void>() {
        @Override
        protected void onPreExecute() {
            IncognitoUtils.closeAllIncognitoTabs();
            Profile.getLastUsedProfile().getOffTheRecordProfile().destroyWhenAppropriate();
        }

        @Override
        protected Void doInBackground() {
            IncognitoUtils.deleteIncognitoStateFiles();
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            openCustomTabActivity();
        }
    };
}
