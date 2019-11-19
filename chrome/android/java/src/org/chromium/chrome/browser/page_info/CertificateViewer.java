// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Typeface;
import android.net.http.SslCertificate;
import android.support.v4.view.ViewCompat;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;

import java.io.ByteArrayInputStream;
import java.security.MessageDigest;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateParsingException;
import java.security.cert.X509Certificate;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * UI component for displaying certificate information.
 */
class CertificateViewer implements OnItemSelectedListener {
    private static final String X_509 = "X.509";
    private static final int SUBJECTALTERNATIVENAME_DNSNAME_ID = 2;
    private static final int SUBJECTALTERNATIVENAME_IPADDRESS_ID = 7;

    private final Context mContext;
    private final int mPadding;
    private ArrayList<String> mTitles;
    private ArrayList<LinearLayout> mViews;
    private CertificateFactory mCertificateFactory;
    private Dialog mDialog;

    public CertificateViewer(Context context) {
        mContext = context;
        mPadding =
                (int) context.getResources().getDimension(R.dimen.connection_info_padding_wide) / 2;
        mDialog = null;
    }

    /**
     * Show a dialog with the provided certificate information.
     * Dialog will contain spinner allowing the user to select
     * which certificate to display.
     *
     * @param derData DER-encoded data representing a X509 certificate chain.
     */
    public void showCertificateChain(byte[][] derData) {
        if (mDialog != null && mDialog.isShowing()) {
            return;
        }

        mTitles = new ArrayList<String>();
        mViews = new ArrayList<LinearLayout>();
        for (int i = 0; i < derData.length; i++) {
            addCertificate(derData[i]);
        }

        ArrayAdapter<String> arrayAdapter = new ArrayAdapter<String>(mContext,
                android.R.layout.simple_spinner_item,
                mTitles) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                TextView view = (TextView) super.getView(position, convertView, parent);
                // Add extra padding on the end side to avoid overlapping the dropdown arrow.
                ViewCompat.setPaddingRelative(view, mPadding, mPadding, mPadding * 2, mPadding);
                return view;
            }
        };
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        LinearLayout dialogContainer = new LinearLayout(mContext);
        dialogContainer.setOrientation(LinearLayout.VERTICAL);

        TextView title = new TextView(mContext);
        title.setText(R.string.certtitle);
        title.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        ApiCompatibilityUtils.setTextAppearance(title, android.R.style.TextAppearance_Large);
        title.setTypeface(title.getTypeface(), Typeface.BOLD);
        title.setPadding(mPadding, mPadding, mPadding, mPadding / 2);
        dialogContainer.addView(title);

        Spinner spinner = new Spinner(mContext);
        spinner.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        spinner.setAdapter(arrayAdapter);
        spinner.setOnItemSelectedListener(this);
        spinner.setDropDownWidth(ViewGroup.LayoutParams.MATCH_PARENT);
        // Remove padding so that dropdown has same width as the spinner.
        spinner.setPadding(0, 0, 0, 0);
        dialogContainer.addView(spinner);

        LinearLayout certContainer = new LinearLayout(mContext);
        certContainer.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < mViews.size(); ++i) {
            LinearLayout certificateView = mViews.get(i);
            if (i != 0) {
                certificateView.setVisibility(LinearLayout.GONE);
            }
            certContainer.addView(certificateView);
        }
        ScrollView scrollView = new ScrollView(mContext);
        scrollView.addView(certContainer);
        dialogContainer.addView(scrollView);

        mDialog = new Dialog(mContext);
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.addContentView(dialogContainer,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));
        mDialog.show();
    }

    private void addCertificate(byte[] derData) {
        try {
            if (mCertificateFactory == null) {
                mCertificateFactory = CertificateFactory.getInstance(X_509);
            }
            Certificate cert = mCertificateFactory.generateCertificate(
                    new ByteArrayInputStream(derData));
            addCertificateDetails(cert, getDigest(derData, "SHA-256"), getDigest(derData, "SHA-1"));
        } catch (CertificateException e) {
            Log.e("CertViewer", "Error parsing certificate" + e.toString());
        }
    }

    private void addCertificateDetails(Certificate cert, byte[] sha256Digest, byte[] sha1Digest) {
        LinearLayout certificateView = new LinearLayout(mContext);
        mViews.add(certificateView);
        certificateView.setOrientation(LinearLayout.VERTICAL);

        X509Certificate x509 = (X509Certificate) cert;
        SslCertificate sslCert = new SslCertificate(x509);

        mTitles.add(sslCert.getIssuedTo().getCName());

        addSectionTitle(certificateView, CertificateViewerJni.get().getCertIssuedToText());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoCommonNameText(),
                sslCert.getIssuedTo().getCName());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoOrganizationText(),
                sslCert.getIssuedTo().getOName());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoOrganizationUnitText(),
                sslCert.getIssuedTo().getUName());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoSerialNumberText(),
                formatBytes(x509.getSerialNumber().toByteArray(), ':'));

        addSectionTitle(certificateView, CertificateViewerJni.get().getCertIssuedByText());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoCommonNameText(),
                sslCert.getIssuedBy().getCName());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoOrganizationText(),
                sslCert.getIssuedBy().getOName());
        addItem(certificateView, CertificateViewerJni.get().getCertInfoOrganizationUnitText(),
                sslCert.getIssuedBy().getUName());

        addSectionTitle(certificateView, CertificateViewerJni.get().getCertValidityText());
        DateFormat dateFormat = DateFormat.getDateInstance(DateFormat.MEDIUM);
        addItem(certificateView, CertificateViewerJni.get().getCertIssuedOnText(),
                dateFormat.format(sslCert.getValidNotBeforeDate()));
        addItem(certificateView, CertificateViewerJni.get().getCertExpiresOnText(),
                dateFormat.format(sslCert.getValidNotAfterDate()));

        addSectionTitle(certificateView, CertificateViewerJni.get().getCertFingerprintsText());
        addItem(certificateView, CertificateViewerJni.get().getCertSHA256FingerprintText(),
                formatBytes(sha256Digest, ' '));
        addItem(certificateView, CertificateViewerJni.get().getCertSHA1FingerprintText(),
                formatBytes(sha1Digest, ' '));

        List<String> subjectAltNames = getSubjectAlternativeNames(x509);
        if (!subjectAltNames.isEmpty()) {
            addSectionTitle(certificateView, CertificateViewerJni.get().getCertExtensionText());
            addLabel(certificateView, CertificateViewerJni.get().getCertSANText());
            for (String name : subjectAltNames) {
                addValue(certificateView, name);
            }
        }
    }

    private void addSectionTitle(LinearLayout certificateView, String label) {
        TextView title = addLabel(certificateView, label);
        title.setAllCaps(true);
    }

    private void addItem(LinearLayout certificateView, String label, String value) {
        if (value.isEmpty()) return;

        addLabel(certificateView, label);
        addValue(certificateView, value);
    }

    private TextView addLabel(LinearLayout certificateView, String label) {
        TextView t = new TextView(mContext);
        t.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        t.setPadding(mPadding, mPadding / 2, mPadding, 0);
        t.setText(label);
        ApiCompatibilityUtils.setTextAppearance(t, R.style.TextAppearance_BlackTitle2);
        certificateView.addView(t);
        return t;
    }

    private void addValue(LinearLayout certificateView, String value) {
        TextView t = new TextView(mContext);
        t.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        t.setText(value);
        t.setPadding(mPadding, 0, mPadding, mPadding / 2);
        ApiCompatibilityUtils.setTextAppearance(t, R.style.TextAppearance_BlackBodyDefault);
        certificateView.addView(t);
    }

    private static String formatBytes(byte[] bytes, char separator) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < bytes.length; i++) {
            sb.append(String.format("%02X", bytes[i]));
            if (i != bytes.length - 1) {
                sb.append(separator);
            }
        }
        return sb.toString();
    }

    private static byte[] getDigest(byte[] bytes, String algorithm) {
        try {
            MessageDigest md = MessageDigest.getInstance(algorithm);
            md.update(bytes);
            return md.digest();
        } catch (java.security.NoSuchAlgorithmException e) {
            return null;
        }
    }

    private static List<String> getSubjectAlternativeNames(X509Certificate x509) {
        List<String> result = new ArrayList<>();
        Collection<List<?>> subjectAltNameList = null;
        try {
            subjectAltNameList = x509.getSubjectAlternativeNames();
        } catch (CertificateParsingException e) {
            // Ignore exception.
        }
        if (subjectAltNameList != null && !subjectAltNameList.isEmpty()) {
            for (List<?> names : subjectAltNameList) {
                if (names == null || names.size() != 2 || names.get(0) == null
                        || names.get(0).getClass() != Integer.class || names.get(1) == null
                        || names.get(1).getClass() != String.class) {
                    continue;
                }
                int id = ((Integer) names.get(0)).intValue();
                if ((id == SUBJECTALTERNATIVENAME_DNSNAME_ID
                            || id == SUBJECTALTERNATIVENAME_IPADDRESS_ID)) {
                    result.add(names.get(1).toString());
                }
            }
        }
        return result;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        for (int i = 0; i < mViews.size(); ++i) {
            mViews.get(i).setVisibility(
                    i == position ? LinearLayout.VISIBLE : LinearLayout.GONE);
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    @NativeMethods
    interface Natives {
        String getCertIssuedToText();
        String getCertInfoCommonNameText();
        String getCertInfoOrganizationText();
        String getCertInfoSerialNumberText();
        String getCertInfoOrganizationUnitText();
        String getCertIssuedByText();
        String getCertValidityText();
        String getCertIssuedOnText();
        String getCertExpiresOnText();
        String getCertFingerprintsText();
        String getCertSHA256FingerprintText();
        String getCertSHA1FingerprintText();
        String getCertExtensionText();
        String getCertSANText();
    }
}
