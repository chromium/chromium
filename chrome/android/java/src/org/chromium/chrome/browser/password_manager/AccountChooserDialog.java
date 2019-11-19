// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.v7.app.AlertDialog;
import android.support.v7.content.res.AppCompatResources;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/**
 *  A dialog offers the user the ability to choose credentials for authentication. User is
 *  presented with username along with avatar and full name in case they are available.
 *  Native counterpart should be notified about credentials user have chosen and also if user
 *  haven't chosen anything.
 */
public class AccountChooserDialog
        implements DialogInterface.OnClickListener, DialogInterface.OnDismissListener {
    private final Context mContext;
    private final Credential[] mCredentials;

    /**
     * Title of the dialog, contains Smart Lock branding for the Smart Lock users.
     */
    private final String mTitle;
    private final int mTitleLinkStart;
    private final int mTitleLinkEnd;
    private final String mOrigin;
    private final String mSigninButtonText;
    private ArrayAdapter<Credential> mAdapter;
    private boolean mIsDestroyed;
    private boolean mWasDismissedByNative;

    /**
     * Holds the reference to the credentials which were chosen by the user.
     */
    private Credential mCredential;
    private long mNativeAccountChooserDialog;
    private AlertDialog mDialog;
    /**
     * True, if credentials were selected via "Sign In" button instead of clicking on the credential
     * itself.
     */
    private boolean mSigninButtonClicked;

    private AccountChooserDialog(Context context, long nativeAccountChooserDialog,
            Credential[] credentials, String title, int titleLinkStart, int titleLinkEnd,
            String origin, String signinButtonText) {
        mNativeAccountChooserDialog = nativeAccountChooserDialog;
        mContext = context;
        mCredentials = credentials.clone();
        mTitle = title;
        mTitleLinkStart = titleLinkStart;
        mTitleLinkEnd = titleLinkEnd;
        mOrigin = origin;
        mSigninButtonText = signinButtonText;
        mSigninButtonClicked = false;
    }

    /**
     *  Creates and shows the dialog which allows user to choose credentials for login.
     *  @param credentials Credentials to display in the dialog.
     *  @param title Title message for the dialog, which can contain Smart Lock branding.
     *  @param titleLinkStart Start of a link in case title contains Smart Lock branding.
     *  @param titleLinkEnd End of a link in case title contains Smart Lock branding.
     *  @param origin Address of the web page, where dialog was triggered.
     */
    @CalledByNative
    private static AccountChooserDialog createAndShowAccountChooser(WindowAndroid windowAndroid,
            long nativeAccountChooserDialog, Credential[] credentials, String title,
            int titleLinkStart, int titleLinkEnd, String origin, String signinButtonText) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;
        AccountChooserDialog chooser =
                new AccountChooserDialog(activity, nativeAccountChooserDialog, credentials, title,
                        titleLinkStart, titleLinkEnd, origin, signinButtonText);
        chooser.show();
        return chooser;
    }

    private ArrayAdapter<Credential> generateAccountsArrayAdapter(
            Context context, Credential[] credentials) {
        return new ArrayAdapter<Credential>(context, 0 /* resource */, credentials) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                if (convertView == null) {
                    LayoutInflater inflater = LayoutInflater.from(getContext());
                    convertView =
                            inflater.inflate(R.layout.account_chooser_dialog_item, parent, false);
                }
                convertView.setTag(position);

                Credential credential = getItem(position);

                ImageView avatarView = (ImageView) convertView.findViewById(R.id.profile_image);
                Drawable avatar = credential.getAvatar();
                if (avatar == null) {
                    avatar = AppCompatResources.getDrawable(
                            getContext(), R.drawable.logo_avatar_anonymous);
                }
                avatarView.setImageDrawable(avatar);

                TextView mainNameView = (TextView) convertView.findViewById(R.id.main_name);
                TextView secondaryNameView =
                        (TextView) convertView.findViewById(R.id.secondary_name);
                if (credential.getFederation().isEmpty()) {
                    // Not federated credentials case
                    if (credential.getDisplayName().isEmpty()) {
                        mainNameView.setText(credential.getUsername());
                        secondaryNameView.setVisibility(View.GONE);
                    } else {
                        mainNameView.setText(credential.getDisplayName());
                        secondaryNameView.setText(credential.getUsername());
                        secondaryNameView.setVisibility(View.VISIBLE);
                    }
                } else {
                    mainNameView.setText(credential.getUsername());
                    secondaryNameView.setText(credential.getFederation());
                    secondaryNameView.setVisibility(View.VISIBLE);
                }

                ImageButton pslInfoButton =
                        (ImageButton) convertView.findViewById(R.id.psl_info_btn);
                final String originUrl = credential.getOriginUrl();

                if (!originUrl.isEmpty()) {
                    pslInfoButton.setVisibility(View.VISIBLE);
                    pslInfoButton.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            showTooltip(view, UrlFormatter.formatUrlForSecurityDisplay(originUrl),
                                    R.layout.material_tooltip);
                        }
                    });
                }

                return convertView;
            }
        };
    }

    private void show() {
        View titleView =
                LayoutInflater.from(mContext).inflate(R.layout.account_chooser_dialog_title, null);
        TextView origin = (TextView) titleView.findViewById(R.id.origin);
        origin.setText(mOrigin);
        TextView titleMessageText = (TextView) titleView.findViewById(R.id.title);
        if (mTitleLinkStart != 0 && mTitleLinkEnd != 0) {
            SpannableString spanableTitle = new SpannableString(mTitle);
            spanableTitle.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    AccountChooserDialogJni.get().onLinkClicked(
                            mNativeAccountChooserDialog, AccountChooserDialog.this);
                    mDialog.dismiss();
                }
            }, mTitleLinkStart, mTitleLinkEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            titleMessageText.setText(spanableTitle, TextView.BufferType.SPANNABLE);
            titleMessageText.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            titleMessageText.setText(mTitle);
        }
        mAdapter = generateAccountsArrayAdapter(mContext, mCredentials);
        final AlertDialog.Builder builder =
                new AlertDialog.Builder(mContext, R.style.Theme_Chromium_AlertDialog)
                        .setCustomTitle(titleView)
                        .setNegativeButton(R.string.cancel, this)
                        .setAdapter(mAdapter, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int item) {
                                mCredential = mCredentials[item];
                            }
                        });
        if (!TextUtils.isEmpty(mSigninButtonText)) {
            builder.setPositiveButton(mSigninButtonText, this);
        }
        mDialog = builder.create();
        mDialog.setOnDismissListener(this);
        mDialog.show();
    }

    private void showTooltip(View view, String message, int layoutId) {
        Context context = view.getContext();
        Resources resources = context.getResources();
        LayoutInflater inflater = LayoutInflater.from(context);

        TextView text = (TextView) inflater.inflate(layoutId, null);
        text.setText(message);
        text.announceForAccessibility(message);

        // This is a work-around for a bug on Android versions KitKat and below
        // (http://crbug.com/693076). The tooltip wouldn't be shown otherwise.
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP) {
            text.setSingleLine(false);
        }

        // The tooltip should be shown above and to the left (right for RTL) of the info button.
        // In order to do so the tooltip's location on the screen is determined. This location is
        // specified with regard to the top left corner and ignores RTL layouts. For this reason the
        // location of the tooltip is also specified as offsets to the top left corner of the
        // screen. Since the tooltip should be shown above the info button, the height of the
        // tooltip needs to be measured. Furthermore, the height of the statusbar is ignored when
        // obtaining the icon's screen location, but must be considered when specifying a y offset.
        // In addition, the measured width is needed in LTR layout, so that the right end of the
        // tooltip aligns with the right end of the info icon.
        final int[] screenPos = new int[2];
        view.getLocationOnScreen(screenPos);

        text.measure(MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        final int width = view.getWidth();

        final int xOffset = view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL
                ? screenPos[0]
                : screenPos[0] + width - text.getMeasuredWidth();

        final int statusBarHeightResourceId =
                resources.getIdentifier("status_bar_height", "dimen", "android");

        final int statusBarHeight = statusBarHeightResourceId > 0
                ? resources.getDimensionPixelSize(statusBarHeightResourceId)
                : 0;

        final int tooltipMargin = resources.getDimensionPixelSize(R.dimen.psl_info_tooltip_margin);

        final int yOffset =
                screenPos[1] - tooltipMargin - statusBarHeight - text.getMeasuredHeight();

        // The xOffset is with regard to the left edge of the screen. Gravity.LEFT is deprecated,
        // which is why the following line is necessary.
        final int xGravity = view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL ? Gravity.END
                                                                                    : Gravity.START;

        Toast toast = new Toast(context);
        toast.setGravity(Gravity.TOP | xGravity, xOffset, yOffset);
        toast.setDuration(Toast.LENGTH_SHORT);
        toast.setView(text);
        toast.show();
    }

    @CalledByNative
    private void imageFetchComplete(int index, Bitmap avatarBitmap) {
        if (mIsDestroyed) return;
        assert index >= 0 && index < mCredentials.length;
        assert mCredentials[index] != null;
        Drawable avatar = ProfileDataCache.makeRoundAvatar(
                mContext.getResources(), avatarBitmap, avatarBitmap.getHeight());
        mCredentials[index].setAvatar(avatar);
        ListView view = mDialog.getListView();
        if (index >= view.getFirstVisiblePosition() && index <= view.getLastVisiblePosition()) {
            // Profile image is in the visible range.
            View credentialView = view.getChildAt(index - view.getFirstVisiblePosition());
            if (credentialView == null) return;
            ImageView avatarView = (ImageView) credentialView.findViewById(R.id.profile_image);
            avatarView.setImageDrawable(avatar);
        }
    }

    private void destroy() {
        assert mNativeAccountChooserDialog != 0;
        assert !mIsDestroyed;
        mIsDestroyed = true;
        AccountChooserDialogJni.get().destroy(
                mNativeAccountChooserDialog, AccountChooserDialog.this);
        mNativeAccountChooserDialog = 0;
        mDialog = null;
    }

    @CalledByNative
    private void dismissDialog() {
        assert !mWasDismissedByNative;
        mWasDismissedByNative = true;
        mDialog.dismiss();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton) {
        if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            mCredential = mCredentials[0];
            mSigninButtonClicked = true;
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (!mWasDismissedByNative) {
            if (mCredential != null) {
                AccountChooserDialogJni.get().onCredentialClicked(mNativeAccountChooserDialog,
                        AccountChooserDialog.this, mCredential.getIndex(), mSigninButtonClicked);
            } else {
                AccountChooserDialogJni.get().cancelDialog(
                        mNativeAccountChooserDialog, AccountChooserDialog.this);
            }
        }
        destroy();
    }

    @NativeMethods
    interface Natives {
        void onCredentialClicked(long nativeAccountChooserDialogAndroid,
                AccountChooserDialog caller, int credentialId, boolean signinButtonClicked);
        void cancelDialog(long nativeAccountChooserDialogAndroid, AccountChooserDialog caller);
        void destroy(long nativeAccountChooserDialogAndroid, AccountChooserDialog caller);
        void onLinkClicked(long nativeAccountChooserDialogAndroid, AccountChooserDialog caller);
    }
}
