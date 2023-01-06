package com.ark.browser.ui.fragment.settings.privacy;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.View;

import androidx.annotation.NonNull;

import com.zpj.fragmentation.dialog.IDialog;
import com.zpj.fragmentation.dialog.impl.AlertDialogFragment;
import com.zpj.fragmentation.dialog.utils.DialogThemeUtils;
import com.zpj.toast.ZToast;
import com.zpj.utils.PrefsHelper;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Informs the user about the existence of other forms of browsing history.
 */
public class OtherFormsOfHistoryDialogFragment extends AlertDialogFragment<OtherFormsOfHistoryDialogFragment> {

    public static final String PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN =
            "PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN";

    private static final String TAG = "OtherFormsOfHistoryDialogFragment";

    public OtherFormsOfHistoryDialogFragment() {
        setTitle(R.string.clear_browsing_data_history_dialog_title);
        setCancelable(false);
        setCancelableInTouchOutside(false);
        hideCancelBtn();
        setPositiveButton(R.string.ok_got_it, (fragment, which) -> {
            // Remember that the dialog about other forms of browsing history has been shown
            // to the user.
            recordDialogWasShown(true);

            // Finishes the ClearBrowsingDataPreferences activity that created this dialog.
            getActivity().finish();
        });
    }

    @Override
    protected View createContentView(CharSequence content) {
        TextViewWithClickableSpans textView = new TextViewWithClickableSpans(context);

        final SpannableString textWithLink = SpanApplier.applySpans(
                getString(R.string.clear_browsing_data_history_dialog_data_text),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(context, (widget) -> {
                            ZToast.warning(UrlConstants.MY_ACTIVITY_URL_IN_CBD_NOTICE);
//                            new TabDelegate(false /* incognito */)
//                                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD_NOTICE,
//                                            TabLaunchType.FROM_CHROME_UI);
                        })));

        textView.setText(textWithLink);
        textView.setTextColor(DialogThemeUtils.getNormalTextColor(context));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
        int padding = ScreenUtils.dp2pxInt(context, 24);
        textView.setPadding(padding, padding / 3, padding, padding / 3);

        return textView;
    }

    /**
     * Sets the preference indicating whether this dialog was already shown.
     * @param shown Whether the dialog was shown.
     */
    private static void recordDialogWasShown(boolean shown) {
        PrefsHelper.with().putBoolean(PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN, shown);
    }

    /**
     * @return Whether the dialog has already been shown to the user before.
     */
    static boolean wasDialogShown() {
        return PrefsHelper.with().getBoolean(
                PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN, false);
    }

}

