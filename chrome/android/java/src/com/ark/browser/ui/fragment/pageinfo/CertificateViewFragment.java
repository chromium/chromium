package com.ark.browser.ui.fragment.pageinfo;

import android.graphics.Typeface;
import android.net.http.SslCertificate;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.settings.Keys;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.page_info.CertificateViewer;

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

public class CertificateViewFragment extends BaseSwipeBackFragment {

    private static final String X_509 = "X.509";
    private static final int SUBJECTALTERNATIVENAME_DNSNAME_ID = 2;
    private static final int SUBJECTALTERNATIVENAME_IPADDRESS_ID = 7;

    private final ArrayList<LinearLayout> mViews = new ArrayList<>();
    private final ArrayList<String> mTitles = new ArrayList<>();

    private int mPadding;

    private ArkWebContents mArkWeb;

    private CertificateFactory mCertificateFactory;

    private LinearLayout llContainer;

    public static CertificateViewFragment newInstance(int pageId) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, pageId);
        CertificateViewFragment fragment = new CertificateViewFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int pageId = Tab.INVALID_PAGE_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                pageId = getArguments().getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
            }
        } else {
            pageId = savedInstanceState.getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
        }
        mArkWeb = ArkWebManager.get(pageId);
        if (mArkWeb == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mArkWeb != null) {
            outState.putInt(Keys.KEY_ID, mArkWeb.getId());
        }
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_certificate_view;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        if (mArkWeb == null) {
            popThis();
            return;
        }
        byte[][] certChain = CertificateChainHelper.getCertificateChain(mArkWeb.getWebContents());
        if (certChain == null) {
            // The WebContents may have been destroyed/invalidated. If so,
            // ignore this request.
            popThis();
            return;
        }

        super.initView(view, savedInstanceState);

        setToolbarTitle(getString(R.string.certtitle));
        mPadding = ScreenUtils.dp2pxInt(12);
        llContainer = view.findViewById(R.id.ll_container);
        showCertificateChain(certChain);
    }

    private void showCertificateChain(byte[][] derData) {
        for (byte[] derDatum : derData) {
            addCertificate(derDatum);
        }
        ArrayAdapter<String> arrayAdapter = new ArrayAdapter<String>(context,
                android.R.layout.simple_spinner_item,
                mTitles) {
            @NonNull
            @Override
            public View getView(int position, View convertView, @NonNull ViewGroup parent) {
                TextView view = (TextView) super.getView(position, convertView, parent);
                // Add extra padding on the end side to avoid overlapping the dropdown arrow.
                view.setPaddingRelative(mPadding, mPadding, mPadding * 2,
                        mPadding);
                return view;
            }
        };
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

//        TextView title = new TextView(context);
//        title.setText(R.string.certtitle);
//        ApiCompatibilityUtils.setTextAlignment(title, View.TEXT_ALIGNMENT_VIEW_START);
//        ApiCompatibilityUtils.setTextAppearance(title, android.R.style.TextAppearance_Large);
//        title.setTypeface(title.getTypeface(), Typeface.BOLD);
//        title.setPadding(mPadding, mPadding, mPadding, mPadding / 2);
//        llContainer.addView(title);

        Spinner spinner = new Spinner(context);
        spinner.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        spinner.setAdapter(arrayAdapter);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
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
        });
        spinner.setDropDownWidth(ViewGroup.LayoutParams.MATCH_PARENT);
        // Remove padding so that dropdown has same width as the spinner.
        spinner.setPadding(0, 0, 0, 0);
        llContainer.addView(spinner);

        LinearLayout certContainer = new LinearLayout(context);
        certContainer.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < mViews.size(); ++i) {
            LinearLayout certificateView = mViews.get(i);
            if (i != 0) {
                certificateView.setVisibility(LinearLayout.GONE);
            }
            certContainer.addView(certificateView);
        }

        llContainer.addView(certContainer);
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
        LinearLayout certificateView = new LinearLayout(context);
        mViews.add(certificateView);
        certificateView.setOrientation(LinearLayout.VERTICAL);

        X509Certificate x509 = (X509Certificate) cert;
        SslCertificate sslCert = new SslCertificate(x509);

        mTitles.add(sslCert.getIssuedTo().getCName());

        addSectionTitle(certificateView, CertificateViewer.nativeGetCertIssuedToText());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoCommonNameText(),
                sslCert.getIssuedTo().getCName());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoOrganizationText(),
                sslCert.getIssuedTo().getOName());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoOrganizationUnitText(),
                sslCert.getIssuedTo().getUName());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoSerialNumberText(),
                formatBytes(x509.getSerialNumber().toByteArray(), ':'));

        addSectionTitle(certificateView, CertificateViewer.nativeGetCertIssuedByText());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoCommonNameText(),
                sslCert.getIssuedBy().getCName());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoOrganizationText(),
                sslCert.getIssuedBy().getOName());
        addItem(certificateView, CertificateViewer.nativeGetCertInfoOrganizationUnitText(),
                sslCert.getIssuedBy().getUName());

        addSectionTitle(certificateView, CertificateViewer.nativeGetCertValidityText());
        DateFormat dateFormat = DateFormat.getDateInstance(DateFormat.MEDIUM);
        addItem(certificateView, CertificateViewer.nativeGetCertIssuedOnText(),
                dateFormat.format(sslCert.getValidNotBeforeDate()));
        addItem(certificateView, CertificateViewer.nativeGetCertExpiresOnText(),
                dateFormat.format(sslCert.getValidNotAfterDate()));

        addSectionTitle(certificateView, CertificateViewer.nativeGetCertFingerprintsText());
        addItem(certificateView, CertificateViewer.nativeGetCertSHA256FingerprintText(),
                formatBytes(sha256Digest, ' '));
        addItem(certificateView, CertificateViewer.nativeGetCertSHA1FingerprintText(), formatBytes(sha1Digest, ' '));

        List<String> subjectAltNames = getSubjectAlternativeNames(x509);
        if (!subjectAltNames.isEmpty()) {
            addSectionTitle(certificateView, CertificateViewer.nativeGetCertExtensionText());
            addLabel(certificateView, CertificateViewer.nativeGetCertSANText());
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
        TextView t = new TextView(context);
        t.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        t.setPadding(mPadding, mPadding / 2, mPadding, 0);
        t.setText(label);
        t.setTypeface(Typeface.defaultFromStyle(Typeface.BOLD));
//        t.setTextColor(ApiCompatibilityUtils.getColor(getResources(),
//                R.color.connection_info_popup_text));
        SkinEngine.setTextColor(t, R.attr.textColorMajor);
        certificateView.addView(t);
        return t;
    }

    private void addValue(LinearLayout certificateView, String value) {
        TextView t = new TextView(context);
        t.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        t.setText(value);
        t.setPadding(mPadding, 0, mPadding, mPadding / 2);
//        t.setTextColor(ApiCompatibilityUtils.getColor(getResources(),
//                R.color.connection_info_popup_text));
        SkinEngine.setTextColor(t, R.attr.textColorMajor);
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


}

