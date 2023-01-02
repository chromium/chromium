package com.ark.browser.ui.fragment.pageinfo;

import android.net.Uri;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.settings.Keys;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.settings.website.SingleWebsiteFragment;
import com.ark.browser.utils.FaviconUtil;
import com.zpj.toast.ZToast;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.page_info.ElidedUrlTextView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.ui.base.Clipboard;

public class PageInfoFragment2 extends BaseSwipeBackFragment {
//        implements PageInfoPopup.OnGetPageInfoListener,
//        ConnectionInfoPopup.OnGetConnectionInfoListener {

//    private final List<PageInfoPopup.PageInfoPermissionEntry> mDisplayedPermissions = new ArrayList<>();

    private ArkWebContents mArkWeb;
//    private PageInfoPopup pageInfoPopup;
//    private ConnectionInfoPopup connectionInfoPopup;

    private TextView tvInfo;
    private LinearLayout llSections;

    public static PageInfoFragment2 newInstance(int pageId) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, pageId);
        PageInfoFragment2 fragment = new PageInfoFragment2();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_page_info;
    }

    @Override
    public void onDestroy() {
//        if (pageInfoPopup != null) {
//            pageInfoPopup.destroy();
//        }
//        if (connectionInfoPopup != null) {
//            connectionInfoPopup.destroy();
//        }
        super.onDestroy();
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
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        if (mArkWeb == null) {
            popThis();
            return;
        }
        super.initView(view, savedInstanceState);
        ZToolBar toolBar = findViewById(R.id.tool_bar);
        toolBar.setCenterText("网页信息");

        findViewById(R.id.tv_certificate).setOnClickListener(v -> {
            start(CertificateViewFragment.newInstance(mArkWeb.getId()));
        });

        findViewById(R.id.tv_permission).setOnClickListener(v -> {
            start(SingleWebsiteFragment.newInstance(mArkWeb.getPageInfo()));
        });

        ElidedUrlTextView tvUrl = view.findViewById(R.id.tv_url);

//        tvUrl.setProfile(tab.getProfile());
//        tvUrl.setAlwaysShowFullUrl(true);
        tvUrl.setOnClickListener(v -> tvUrl.toggleTruncation());
        // Long press the url text to copy it to the clipboard.
        tvUrl.setOnLongClickListener(v -> {
            Clipboard.getInstance().setTextFromUser(tvUrl.getText().toString());
            ZToast.success(R.string.text_url_copied);
            return true;
        });

//        String mFullUrl = tab.getOriginalUrl();
//
//        // This can happen if an invalid chrome-distiller:// url was entered.
//        if (mFullUrl == null) mFullUrl = "";
//
//        if (isShowingOfflinePage()) {
//            mFullUrl = OfflinePageUtils.stripSchemeFromOnlineUrl(mFullUrl);
//        }

        boolean mIsInternalPage = UrlUtilities.isInternalScheme(mArkWeb.getUrl());
        int mSecurityLevel = SecurityStateModel.getSecurityLevelForWebContents(mArkWeb.getWebContents());
        SpannableStringBuilder urlBuilder = new SpannableStringBuilder(mArkWeb.getUrl().getSpec());
        AutocompleteSchemeClassifier autocompleteSchemeClassifier = new ChromeAutocompleteSchemeClassifier(
                mArkWeb.getProfile());
        OmniboxUrlEmphasizer.emphasizeUrl(urlBuilder, context, autocompleteSchemeClassifier,
                mSecurityLevel, mIsInternalPage, !AppConfig.isNightMode(), true);

        int urlOriginLength = OmniboxUrlEmphasizer.getOriginEndIndex(
                urlBuilder.toString(), autocompleteSchemeClassifier);
        tvUrl.setUrl(urlBuilder, urlOriginLength);
        autocompleteSchemeClassifier.destroy();

        if (mSecurityLevel == ConnectionSecurityLevel.SECURE) {
//            OmniboxUrlEmphasizer.EmphasizeComponentsResponse emphasizeResponse =
//                    OmniboxUrlEmphasizer.parseForEmphasizeComponents(
//                            tab.getProfile(), urlBuilder.toString());
//            if (emphasizeResponse.schemeLength > 0) {
//                urlBuilder.setSpan(
//                        new TextAppearanceSpan(context, R.style.RobotoMediumStyle),
//                        0, emphasizeResponse.schemeLength, Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
//            }
            String scheme = Uri.parse(urlBuilder.toString()).getScheme();
            if (!TextUtils.isEmpty(scheme)) {
                urlBuilder.setSpan(
                        new TextAppearanceSpan(context, R.style.RobotoMediumStyle),
                        0, scheme.length(), Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }
        tvUrl.setText(urlBuilder);

        ImageView ivIcon = view.findViewById(R.id.iv_icon);
        tvInfo = view.findViewById(R.id.tv_info);
        llSections = view.findViewById(R.id.ll_sections);

        FaviconUtil.with(getContext(), mArkWeb.getUrl().getSpec())
                .setCallback(ivIcon::setImageDrawable)
                .start();

//        pageInfoPopup = PageInfoPopup.create(_mActivity, tab, null);
//        pageInfoPopup.setOnGetPageInfoListener(this);
//        connectionInfoPopup = new ConnectionInfoPopup(context, tab.getWebContents());
//        connectionInfoPopup.setOnGetConnectionInfoListener(this);
//
//        pageInfoPopup.init();
//        connectionInfoPopup.init();
    }

//    @Override
//    public void onAddPermissionSection(PageInfoPopup.PageInfoPermissionEntry entry) {
//        mDisplayedPermissions.add(entry);
//    }
//
//    @Override
//    public void onUpdatePermissionDisplay() {
//
//    }
//
//    @Override
//    public void onSetSecurityDescription(CharSequence messageBuilder, boolean isConnectionDetailsLinkVisible) {
//        tvInfo.setText(messageBuilder);
//    }
//
//    @Override
//    public void onAddCertificateSection(View section) {
//        llSections.addView(section);
//    }
//
//    @Override
//    public void onAddDescriptionSection(View section) {
//        llSections.addView(section);
//    }
//
//    @Override
//    public void onAddResetCertDecisionsButton(View button) {
//        llSections.addView(button);
//    }
//
//    @Override
//    public void onAddMoreInfoLink(TextView moreInfoLink) {
//
//    }

}

