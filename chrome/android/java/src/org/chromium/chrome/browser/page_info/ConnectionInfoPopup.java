// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.vr.UiUnsupportedMode;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Java side of Android implementation of the page info UI.
 */
public class ConnectionInfoPopup implements OnClickListener, ModalDialogProperties.Controller {
    private static final String TAG = "ConnectionInfoPopup";

    private static final String HELP_URL =
            "https://support.google.com/chrome?p=android_connection_info";

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private PropertyModel mDialogModel;
    private final LinearLayout mContainer;
    private final WebContents mWebContents;
    private final WebContentsObserver mWebContentsObserver;
    private final int mPaddingWide, mPaddingThin;
    private final long mNativeConnectionInfoPopup;
    private final CertificateViewer mCertificateViewer;
    private TextView mCertificateViewerTextView, mMoreInfoLink;
    private ViewGroup mCertificateLayout, mDescriptionLayout;
    private Button mResetCertDecisionsButton;
    private String mLinkUrl;

    private ConnectionInfoPopup(Context context, Tab tab) {
        mContext = context;
        mModalDialogManager = tab.getActivity().getModalDialogManager();
        mWebContents = tab.getWebContents();

        mCertificateViewer = new CertificateViewer(mContext);

        mContainer = new LinearLayout(mContext);
        mContainer.setOrientation(LinearLayout.VERTICAL);
        mPaddingWide = (int) context.getResources().getDimension(
                R.dimen.connection_info_padding_wide);
        mPaddingThin = (int) context.getResources().getDimension(
                R.dimen.connection_info_padding_thin);
        mContainer.setPadding(mPaddingWide, mPaddingWide, mPaddingWide,
                mPaddingWide - mPaddingThin);

        // This needs to come after other member initialization.
        mNativeConnectionInfoPopup = ConnectionInfoPopupJni.get().init(this, mWebContents);
        mWebContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're
                // showing is stale so dismiss the dialog.
                dismissDialog(DialogDismissalCause.UNKNOWN);
            }

            @Override
            public void destroy() {
                super.destroy();
                dismissDialog(DialogDismissalCause.UNKNOWN);
            }
        };
    }

    /**
     * Adds certificate section, which contains an icon, a headline, a
     * description and a label for certificate info link.
     */
    @CalledByNative
    private void addCertificateSection(int enumeratedIconId, String headline, String description,
            String label) {
        View section = addSection(enumeratedIconId, headline, description);
        assert mCertificateLayout == null;
        mCertificateLayout = (ViewGroup) section.findViewById(R.id.connection_info_text_layout);
        if (label != null && !label.isEmpty()) {
            setCertificateViewer(label);
        }
    }

    /**
     * Adds Description section, which contains an icon, a headline, and a
     * description. Most likely headline for description is empty
     */
    @CalledByNative
    private void addDescriptionSection(int enumeratedIconId, String headline, String description) {
        View section = addSection(enumeratedIconId, headline, description);
        assert mDescriptionLayout == null;
        mDescriptionLayout = section.findViewById(R.id.connection_info_text_layout);
    }

    private View addSection(int enumeratedIconId, String headline, String description) {
        View section = LayoutInflater.from(mContext).inflate(R.layout.connection_info,
                null);
        ImageView i = section.findViewById(R.id.connection_info_icon);
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        i.setImageResource(drawableId);

        TextView h = section.findViewById(R.id.connection_info_headline);
        h.setText(headline);
        if (TextUtils.isEmpty(headline)) h.setVisibility(View.GONE);

        TextView d = section.findViewById(R.id.connection_info_description);
        d.setText(description);
        if (TextUtils.isEmpty(description)) d.setVisibility(View.GONE);

        mContainer.addView(section);
        return section;
    }

    private void setCertificateViewer(String label) {
        assert mCertificateViewerTextView == null;
        mCertificateViewerTextView = new TextView(mContext);
        mCertificateViewerTextView.setText(label);
        ApiCompatibilityUtils.setTextAppearance(
                mCertificateViewerTextView, R.style.TextAppearance_BlueLink3);
        mCertificateViewerTextView.setOnClickListener(this);
        mCertificateViewerTextView.setPadding(0, mPaddingThin, 0, 0);
        mCertificateLayout.addView(mCertificateViewerTextView);
    }

    private void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    @CalledByNative
    private void addResetCertDecisionsButton(String label) {
        assert mResetCertDecisionsButton == null;

        mResetCertDecisionsButton = new ButtonCompat(mContext, R.style.FilledButtonThemeOverlay);
        mResetCertDecisionsButton.setText(label);
        mResetCertDecisionsButton.setOnClickListener(this);

        LinearLayout container = new LinearLayout(mContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.addView(mResetCertDecisionsButton);
        container.setPadding(0, 0, 0, mPaddingWide);
        mContainer.addView(container);
    }

    @CalledByNative
    private void addMoreInfoLink(String linkText) {
        mMoreInfoLink = new TextView(mContext);
        mLinkUrl = HELP_URL;
        mMoreInfoLink.setText(linkText);
        ApiCompatibilityUtils.setTextAppearance(mMoreInfoLink, R.style.TextAppearance_BlueLink3);
        mMoreInfoLink.setPadding(0, mPaddingThin, 0, 0);
        mMoreInfoLink.setOnClickListener(this);
        mDescriptionLayout.addView(mMoreInfoLink);
    }

    /** Displays the ConnectionInfoPopup. */
    @CalledByNative
    private void showDialog() {
        ScrollView scrollView = new ScrollView(mContext);
        scrollView.addView(mContainer);

        mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                               .with(ModalDialogProperties.CONTROLLER, this)
                               .with(ModalDialogProperties.CUSTOM_VIEW, scrollView)
                               .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                               .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    @Override
    public void onClick(View v) {
        if (mResetCertDecisionsButton == v) {
            ConnectionInfoPopupJni.get().resetCertDecisions(
                    mNativeConnectionInfoPopup, ConnectionInfoPopup.this, mWebContents);
            dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        } else if (mCertificateViewerTextView == v) {
            byte[][] certChain = CertificateChainHelper.getCertificateChain(mWebContents);
            if (certChain == null) {
                // The WebContents may have been destroyed/invalidated. If so,
                // ignore this request.
                return;
            }
            if (VrModuleProvider.getDelegate().isInVr()) {
                VrModuleProvider.getDelegate().requestToExitVrAndRunOnSuccess(() -> {
                    mCertificateViewer.showCertificateChain(certChain);
                }, UiUnsupportedMode.UNHANDLED_CERTIFICATE_INFO);
                return;
            }
            mCertificateViewer.showCertificateChain(certChain);
        } else if (mMoreInfoLink == v) {
            if (VrModuleProvider.getDelegate().isInVr()) {
                VrModuleProvider.getDelegate().requestToExitVrAndRunOnSuccess(
                        this ::showConnectionSecurityInfo,
                        UiUnsupportedMode.UNHANDLED_CONNECTION_SECURITY_INFO);
                return;
            }
            showConnectionSecurityInfo();
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        assert mNativeConnectionInfoPopup != 0;
        mWebContentsObserver.destroy();
        ConnectionInfoPopupJni.get().destroy(mNativeConnectionInfoPopup, ConnectionInfoPopup.this);
        mDialogModel = null;
    }

    private void showConnectionSecurityInfo() {
        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        try {
            Intent i = Intent.parseUri(mLinkUrl, Intent.URI_INTENT_SCHEME);
            i.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
            i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
            mContext.startActivity(i);
        } catch (Exception ex) {
            // Do nothing intentionally.
            Log.w(TAG, "Bad URI %s", mLinkUrl, ex);
        }
    }

    /**
     * Shows a connection info dialog for the provided WebContents.
     *
     * The popup adds itself to the view hierarchy which owns the reference while it's
     * visible.
     *
     * @param context Context which is used for launching a dialog.
     * @param tab The tab hosting the web contents for which to show website information. This
     *            information is retrieved for the visible entry.
     */
    public static void show(Context context, Tab tab) {
        new ConnectionInfoPopup(context, tab);
    }

    @NativeMethods
    interface Natives {
        long init(ConnectionInfoPopup popup, WebContents webContents);
        void destroy(long nativeConnectionInfoPopupAndroid, ConnectionInfoPopup caller);
        void resetCertDecisions(long nativeConnectionInfoPopupAndroid, ConnectionInfoPopup caller,
                WebContents webContents);
    }
}
