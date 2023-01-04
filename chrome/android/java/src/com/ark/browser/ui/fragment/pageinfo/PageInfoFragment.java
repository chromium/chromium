package com.ark.browser.ui.fragment.pageinfo;

import android.net.Uri;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.settings.Keys;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.settings.website.CookieFragment;
import com.ark.browser.ui.fragment.settings.website.SingleWebsiteFragment;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.FaviconUtil;
import com.zpj.fragmentation.dialog.IDialog;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.page_info.ConnectionInfoView;
import org.chromium.components.page_info.ElidedUrlTextView;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoPageZoomView;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.url.GURL;
import org.chromium.chrome.R;

import java.text.DateFormat;
import java.util.Collection;
import java.util.Date;

public class PageInfoFragment extends BaseSwipeBackFragment {

    private ArkWebContents mArkWeb;
    private PageInfoController mController;
    private GURL mUrl;

    protected String mOfflinePageUrl;
    @PageInfoControllerDelegate.OfflinePageState
    protected int mOfflinePageState = PageInfoControllerDelegate.OfflinePageState.NOT_OFFLINE_PAGE;
    private String mOfflinePageCreationDate;

    private TextView tvInfo;
    private LinearLayout llSections;

    public static PageInfoFragment newInstance(int pageId) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, pageId);
        PageInfoFragment fragment = new PageInfoFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_page_info;
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

        ImageView ivIcon = view.findViewById(R.id.iv_icon);

        findViewById(R.id.tv_permission).setOnClickListener(v -> {
            start(SingleWebsiteFragment.newInstance(mArkWeb.getPageInfo()));
        });

        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(mArkWeb.getWebContents());
        if (offlinePage != null) {
            mOfflinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(mArkWeb.getWebContents())) {
                mOfflinePageState = PageInfoControllerDelegate.OfflinePageState.TRUSTED_OFFLINE_PAGE;
            } else {
                mOfflinePageState = PageInfoControllerDelegate.OfflinePageState.UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                mOfflinePageCreationDate = df.format(creationDate);
            }
        }

        boolean isShowingOfflinePage =
                mOfflinePageState != PageInfoControllerDelegate.OfflinePageState.NOT_OFFLINE_PAGE;

        ArkLogger.e(this, "initView isShowingOfflinePage=" + isShowingOfflinePage
                + " mOfflinePageUrl=" + mOfflinePageUrl + " url=" + mArkWeb.getUrl().getSpec());
        mUrl = isShowingOfflinePage
                ? new GURL(mOfflinePageUrl)
                : DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(mArkWeb.getUrl());
        boolean mIsInternalPage = UrlUtilities.isInternalScheme(mUrl);

        FaviconUtil.with(getContext(), mUrl.getSpec())
                .setCallback(ivIcon::setImageDrawable)
                .start();




        tvInfo = findViewById(R.id.tv_info);
        llSections = findViewById(R.id.ll_sections);

        ElidedUrlTextView tvUrl = findViewById(R.id.tv_url);
        tvUrl.setOnClickListener(v -> tvUrl.toggleTruncation());
        // Long press the url text to copy it to the clipboard.
        tvUrl.setOnLongClickListener(v -> {
            Clipboard.getInstance().setTextFromUser(tvUrl.getText().toString());
            ZToast.success(R.string.text_url_copied);
            return true;
        });


        int mSecurityLevel = SecurityStateModel.getSecurityLevelForWebContents(mArkWeb.getWebContents());
        SpannableStringBuilder urlBuilder = new SpannableStringBuilder(mUrl.getSpec());
        AutocompleteSchemeClassifier autocompleteSchemeClassifier = new ChromeAutocompleteSchemeClassifier(
                mArkWeb.getProfile());
        OmniboxUrlEmphasizer.emphasizeUrl(urlBuilder, context, autocompleteSchemeClassifier,
                mSecurityLevel, mIsInternalPage, !AppConfig.isNightMode(), true);

        int urlOriginLength = OmniboxUrlEmphasizer.getOriginEndIndex(
                urlBuilder.toString(), autocompleteSchemeClassifier);
        tvUrl.setUrl(urlBuilder, urlOriginLength);
        autocompleteSchemeClassifier.destroy();
        if (mSecurityLevel == ConnectionSecurityLevel.SECURE) {
            String scheme = Uri.parse(urlBuilder.toString()).getScheme();
            if (!TextUtils.isEmpty(scheme)) {
                urlBuilder.setSpan(
                        new TextAppearanceSpan(context, R.style.RobotoMediumStyle),
                        0, scheme.length(), Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }
        tvUrl.setText(urlBuilder);


        ConnectionInfoView connectionInfoView = initConnectionInfoView();

        initCookiesView();


//        initPageZoomView();



        mController = new PageInfoController(mArkWeb.getWebContents(), new PageInfoController.PageInfoListener() {
            @Override
            public void onAddPermissionSection(String name, String nameMidSentence, int type, int currentSettingValue) {

            }

            @Override
            public void onUpdatePermissionDisplay() {

            }

            @Override
            public void onSetSecurityDescription(String summary, String details) {
//                tvInfo.setText(summary + "\n" + details);

                SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
                messageBuilder.append(summary);
                messageBuilder.append('\n');
                messageBuilder.append(details);

                boolean isConnectionDetailsLinkVisible = !isShowingOfflinePage && !mIsInternalPage;

                if (isConnectionDetailsLinkVisible && messageBuilder.length() > 0) {
                    messageBuilder.append(" ");
                    SpannableString detailsText =
                            new SpannableString(getString(R.string.details_link));
                    final ForegroundColorSpan blueSpan = new ForegroundColorSpan(
                            SemanticColorUtils.getDefaultTextColorLink(context));
                    detailsText.setSpan(
                            blueSpan, 0, detailsText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
                    messageBuilder.append(detailsText);
                }

                tvInfo.setText(messageBuilder);

//                connectionInfoView.
            }

            @Override
            public void onUpdateTopicsDisplay(String[] topics) {

            }
        });

    }

    @Override
    public void onDestroyView() {
        if (mController != null) {
            mController.destroy();
            mController = null;
        }
        super.onDestroyView();
    }

    private ConnectionInfoView initConnectionInfoView() {
        return ConnectionInfoView.create(context, mArkWeb.getWebContents(),
                new ConnectionInfoView.ConnectionInfoDelegate() {
                    @Override
                    public void onReady(ConnectionInfoView popup) {
                        llSections.addView(popup.getView());
                    }

                    @Override
                    public boolean onShowCertificateChain() {
                        start(CertificateViewFragment.newInstance(mArkWeb.getId()));
                        return true;
                    }

                    @Override
                    public void dismiss(int actionOnContent) {

                    }
                }, null);
    }

    private void initCookiesView() {
        TextView tvCookiesDesc = findViewById(R.id.tv_cookies_desc);
        NoUnderlineClickableSpan linkSpan = new NoUnderlineClickableSpan(
                context, (v) -> start(new CookieFragment()));
        tvCookiesDesc.setText(
                SpanApplier.applySpans(getString(R.string.page_info_cookies_description),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));
        tvCookiesDesc.setMovementMethod(LinkMovementMethod.getInstance());

        CommonSettingItem cookiesItem = findViewById(R.id.item_cookies_usage);

        new WebsitePermissionsFetcher(Profile.getLastUsedRegularProfile())
                .fetchPreferencesForStorage(sites -> {
                    String origin = Origin.createOrThrow(mUrl.getSpec()).toString();
                    WebsiteAddress address = WebsiteAddress.create(origin);

                    Website website = SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                            address, sites);

                    cookiesItem.setOnItemClickListener(new OnCommonItemClickListener() {
                        @Override
                        public void onItemClick(CommonSettingItem commonSettingItem) {
                            ZDialog.alert()
                                    .setTitle(R.string.page_info_cookies_clear)
                                    .setContent(getString(R.string.page_info_cookies_clear_confirmation, mUrl.getHost()))
                                    .setPositiveButton(R.string.clear, new IDialog.OnButtonClickListener<ZDialog.AlertDialogFragmentImpl>() {
                                        @Override
                                        public void onClick(@NonNull ZDialog.AlertDialogFragmentImpl alertDialogFragment, int i) {
                                            website.clearData(Profile.getLastUsedRegularProfile(), new Runnable() {
                                                @Override
                                                public void run() {
                                                    cookiesItem.setOnItemClickListener(null);
                                                }
                                            });
                                        }
                                    })
                                    .setNegativeText(getString(R.string.text_cancel))
                                    .show(context);
                        }
                    });

                });

    }

    private void initPageZoomView() {
        PageInfoPageZoomView pageZoomView = new PageInfoPageZoomView(context, new PageInfoPageZoomView.PageZoomViewDelegate() {
            @Override
            public void setZoomLevel(double newZoomLevel) {
                HostZoomMap.setZoomLevel(mArkWeb.getWebContents(), newZoomLevel);
            }

            @Override
            public double getZoomLevel() {
                return HostZoomMap.getZoomLevel(mArkWeb.getWebContents());
            }
        });

        llSections.addView(pageZoomView.getMainView());
    }

}
