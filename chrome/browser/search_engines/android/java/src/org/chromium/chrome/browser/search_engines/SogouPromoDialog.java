// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.app.Activity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.widget.PromoDialog;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A promotion dialog showing that the default search provider will be set to Sogou. */
public class SogouPromoDialog extends PromoDialog {
    @IntDef({
        UserChoice.USE_SOGOU,
        UserChoice.KEEP_GOOGLE,
        UserChoice.SETTINGS,
        UserChoice.BACK_KEY
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UserChoice {
        int USE_SOGOU = 0;
        int KEEP_GOOGLE = 1;
        int SETTINGS = 2;
        int BACK_KEY = 3;
    }

    /** Run when the dialog is dismissed. */
    private final Callback<Boolean> mOnDismissedCallback;

    /** Called when the search engine to use is selected. */
    private final Callback<Boolean> mOnSelectEngineCallback;

    private final ClickableSpan mSpan;

    private @UserChoice int mChoice = UserChoice.BACK_KEY;

    /** Creates an instance of the dialog. */
    public SogouPromoDialog(
            Activity activity,
            @NonNull Callback<Boolean> onSelectEngine,
            @Nullable Callback<Boolean> onDismissed) {
        super(activity);
        mSpan =
                new NoUnderlineClickableSpan(
                        activity,
                        (widget) -> {
                            mChoice = UserChoice.SETTINGS;
                            SettingsNavigationFactory.createSettingsNavigation()
                                    .startSettings(getContext(), SearchEngineSettings.class);
                            dismiss();
                        });
        setOnDismissListener(this);
        setCanceledOnTouchOutside(false);
        mOnDismissedCallback = onDismissed;
        mOnSelectEngineCallback = onSelectEngine;
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.vectorDrawableResource = R.drawable.search_sogou;
        params.headerStringResource = R.string.search_with_sogou;
        params.subheaderStringResource = R.string.sogou_explanation;
        params.primaryButtonStringResource = R.string.ok;
        params.secondaryButtonStringResource = R.string.keep_google;
        return params;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Do not allow this dialog to be reconstructed because it requires native side loaded.
        if (savedInstanceState != null) {
            dismiss();
            return;
        }

        StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        TextView textView = (TextView) findViewById(R.id.subheader);
        SpannableString description =
                SpanApplier.applySpans(
                        getContext().getString(R.string.sogou_explanation),
                        new SpanInfo("<link>", "</link>", mSpan),
                        new SpanInfo("<b>", "</b>", boldSpan));
        textView.setText(description);
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.button_secondary) {
            mChoice = UserChoice.KEEP_GOOGLE;
        } else if (view.getId() == R.id.button_primary) {
            mChoice = UserChoice.USE_SOGOU;
        } else {
            assert false : "Not handled click.";
        }
        dismiss();
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        switch (mChoice) {
            case UserChoice.KEEP_GOOGLE:
            case UserChoice.SETTINGS:
            case UserChoice.BACK_KEY:
                mOnSelectEngineCallback.onResult(false);
                break;
            case UserChoice.USE_SOGOU:
                mOnSelectEngineCallback.onResult(true);
                break;
            default:
                assert false : "Unexpected choice";
        }
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.LOCALE_MANAGER_PROMO_SHOWN, true);

        if (mOnDismissedCallback != null) mOnDismissedCallback.onResult(true);
    }
}
