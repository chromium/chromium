package com.ark.browser.ui.fragment.dialog;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.core.utils.TabPrinter;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.settings.Keys;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.ui.widget.DrawableTintTextView;
import com.ark.browser.ui.widget.TextCircleImageView;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;
import com.zpj.utils.PrefsHelper;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

public class ToolsDialog extends OverDragBottomDialogFragment<ToolsDialog> implements View.OnClickListener {

    private static final String CHECKED_COLOR_STRING = "#4285F4";

    private TextCircleImageView color_1, color_2, color_3, color_4, color_5;

    private Tab mPage;

    private boolean smartNoImageMode = false;
    private boolean incognitoMode = false;
    private boolean fullscreenMode = false;
    private boolean showConsole = false;
    private boolean refreshTiming = false;
    private boolean mEditMode;

    public static void start(Context context, Tab page) {
        ToolsDialog toolsDialog = new ToolsDialog();
        toolsDialog.mPage = page;
        toolsDialog.show(context);
    }

//    public static ToolsDialog newInstance(int tabId) {
//        Bundle args = new Bundle();
//        args.putInt(Keys.KEY_ID, tabId);
//        ToolsDialog fragment = new ToolsDialog();
//        fragment.setArguments(args);
//        return fragment;
//    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        int pageId = Tab.INVALID_PAGE_ID;
//        if (savedInstanceState == null) {
//            if (getArguments() != null) {
//                pageId = getArguments().getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
//            }
//        } else {
//            pageId = savedInstanceState.getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
//        }
        if (mPage == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_tool_box;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        smartNoImageMode = AppConfig.isSmartNoImageMode();
        incognitoMode = AppConfig.isIncognitoMode();
        fullscreenMode = AppConfig.isFullscreenMode();
        showConsole = AppConfig.isShowConsole();
        refreshTiming = AppConfig.isRefreshTiming();
        mEditMode = AppConfig.isEditMode();

        color_1 = findViewById(R.id.color_1);
        color_1.setText("白");
        color_2 = findViewById(R.id.color_2);
        color_2.setText("黄");
        color_3 = findViewById(R.id.color_3);
        color_3.setText("绿");
        color_4 = findViewById(R.id.color_4);
        color_4.setText("灰");
        color_5 = findViewById(R.id.color_5);
        color_5.setText("橄榄");

        int current_html_backgroundcolor = PrefsHelper.with().getInt("current_html_backgroundcolor", 0);
        switch (current_html_backgroundcolor) {
            case 0:
                onChangeColorClicked(color_1);
                break;
            case 1:
                onChangeColorClicked(color_2);
                break;
            case 2:
                onChangeColorClicked(color_3);
                break;
            case 3:
                onChangeColorClicked(color_4);
                break;
            case 4:
                onChangeColorClicked(color_5);
                break;
        }

        ImageView ibInfo = findViewById(R.id.ib_info);
        ImageView ibClose = findViewById(R.id.ib_close);
        ImageView ibShare = findViewById(R.id.ib_share);
        DrawableTintTextView tvWebsiteSettings = findViewById(R.id.tv_website_settings);
        DrawableTintTextView tvHistoryStack = findViewById(R.id.tv_history_stack);
        DrawableTintTextView tvFullscreen = findViewById(R.id.tv_fullscreen);
        DrawableTintTextView tvPageSearch = findViewById(R.id.tv_page_search);
        DrawableTintTextView tvSaveHtml = findViewById(R.id.tv_save_html);
        DrawableTintTextView tvConsole = findViewById(R.id.tv_console);
        DrawableTintTextView tvRefreshTiming = findViewById(R.id.tv_refresh_timing);
        DrawableTintTextView tvResource = findViewById(R.id.tv_resource);
        DrawableTintTextView tvLog = findViewById(R.id.tv_log);
        DrawableTintTextView tvSeeHtml = findViewById(R.id.tv_see_html);
        DrawableTintTextView tvTranslate = findViewById(R.id.tv_translate);
        DrawableTintTextView tvSmartNoImg = findViewById(R.id.tv_smart_no_img);

        DrawableTintTextView tvReaderMode = findViewById(R.id.tv_reader_mode);
        DrawableTintTextView tvEditMode = findViewById(R.id.tv_edit_mode);

        int primaryColor = getResources().getColor(R.color.colorPrimary);
        if (smartNoImageMode) {
            tvSmartNoImg.setTint(primaryColor);
        }
        if (fullscreenMode) {
            tvFullscreen.setTint(primaryColor);
        }
        if (showConsole) {
            tvConsole.setTint(primaryColor);
        }
        if (refreshTiming) {
            tvRefreshTiming.setTint(primaryColor);
        }
        if (mEditMode) {
            tvEditMode.setTint(primaryColor);
        }

        if (!shouldShowPageMenu()) {
//            int color = Color.parseColor("#cccccc");
            int color = SkinEngine.getColor(context, R.attr.textColorMinor);
            tvPageSearch.setTint(color);
            tvSaveHtml.setTint(color);
            tvResource.setTint(color);
            tvLog.setTint(color);
            tvSeeHtml.setTint(color);
            tvTranslate.setTint(color);
            tvWebsiteSettings.setTint(color);
            ibInfo.setColorFilter(color);
        } else {
            ibShare.setOnClickListener(this);
            tvPageSearch.setOnClickListener(this);
            tvSaveHtml.setOnClickListener(this);
            tvResource.setOnClickListener(this);
            tvLog.setOnClickListener(this);
            tvSeeHtml.setOnClickListener(this);
            tvTranslate.setOnClickListener(this);
            tvWebsiteSettings.setOnClickListener(this);
            ibInfo.setOnClickListener(this);
        }

        tvEditMode.setOnClickListener(this);
        ibClose.setOnClickListener(this);
        tvConsole.setOnClickListener(this);
        tvRefreshTiming.setOnClickListener(this);
        tvHistoryStack.setOnClickListener(this);
        tvFullscreen.setOnClickListener(this);
        tvSmartNoImg.setOnClickListener(this);
        tvReaderMode.setOnClickListener(this);
    }

    private void changeHeight(TextCircleImageView textCircleImageView, int dp) {
        ViewGroup.LayoutParams layoutParams = textCircleImageView.getLayoutParams();
        layoutParams.height = ScreenUtils.dp2pxInt(getContext(), dp);
        textCircleImageView.setLayoutParams(layoutParams);
    }

    private void onChangeColorClicked(TextCircleImageView textCircleImageView) {
        changeHeight(color_1, 32);
        changeHeight(color_2, 32);
        changeHeight(color_3, 32);
        changeHeight(color_4, 32);
        changeHeight(color_5, 32);
        changeHeight(textCircleImageView, 38);
    }

    private void changeHtmlColor(Tab page, TextCircleImageView textCircleImageView, int id) {
        onChangeColorClicked(textCircleImageView);
        PrefsHelper.with().applyInt("current_html_backgroundcolor", id);
        page.evaluateJavaScript("javascript:changeColor(" + PrefsHelper.with().getInt("current_html_backgroundcolor", 0) + ");", null);
    }

    private boolean shouldShowPageMenu() {
        return true;
//        ChromeActivity activity = ChromeActivity.fromContext(context);
//        if (activity.isTablet()) {
//            boolean hasTabs = activity.getLauncherFragment().getCurrentTabModel().getCount() != 0;
//            return hasTabs;
//        } else {
//            return activity.getLauncherFragment().isInBrowser();
//        }
    }

    @SuppressLint("NonConstantResourceId")
    @Override
    public void onClick(View v) {
        Tab page = mPage;
        int id = v.getId();
        if (id == R.id.color_1) {
            changeHtmlColor(page, color_1, 0);
        } else if (id == R.id.color_2) {
            changeHtmlColor(page, color_2, 1);
        } else if (id == R.id.color_3) {
            changeHtmlColor(page, color_3, 2);
        } else if (id == R.id.color_4) {
            changeHtmlColor(page, color_4, 3);
        } else if (id == R.id.color_5) {
            changeHtmlColor(page, color_5, 4);
        } else if (id == R.id.ib_share) {
            ShareDialog.start(context);
        } else if (id == R.id.ib_info) {
//            PageInfoFragment.newInstance(mPageInfo.getPageId()).show(context);
        } else if (id == R.id.tv_website_settings) {
//            SingleWebsiteFragment.start(mPageInfo);
        } else if (id == R.id.tv_history_stack) {
            HistoryStackDialogFragment.newInstance(mPage).show(context);
        } else if (id == R.id.tv_fullscreen) {
            boolean tag = !fullscreenMode;
            AppConfig.toggleFullscreenMode();
            if (tag) {
                _mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            } else {
                _mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            }

            DrawableTintTextView tvFullscreen = (DrawableTintTextView) v;
            tvFullscreen.setTint(getResources().getColor(tag ? R.color.colorPrimary : R.color.google_black_400));
//                GetLauncherEvent.post(launcherFragment -> launcherFragment.getStatusBarView().setVisibility(tag ? View.GONE : View.VISIBLE));
        } else if (id == R.id.tv_page_search) {
//            GetLauncherEvent.post(LauncherFragment::showFindInPageToolbar);
        } else if (id == R.id.tv_save_html) {
            OfflinePageOrigin origin = new OfflinePageOrigin(getContext(), page);
            if (page.isShowingErrorPage()) {
                // The download needs to be scheduled to happen at later time due to current network
                // error.
                final OfflinePageBridge bridge = OfflinePageBridge.getForProfile(Profile.getLastUsedRegularProfile());
                bridge.scheduleDownload(page.getWebContents(), OfflinePageBridge.ASYNC_NAMESPACE,
                        page.getUrl().getSpec(), DownloadUiActionFlags.PROMPT_DUPLICATE, origin);
            } else {
                // Otherwise, the download can be started immediately.
                // TODO 先申请读写权限
                OfflinePageDownloadBridge.startDownload(page, origin);
//                                    DownloadUtils.recordDownloadPageMetrics(mTab);
            }
        } else if (id == R.id.tv_console) {
            boolean tag = !showConsole;
            if (tag) {
                initVConsole();
            }
            AppConfig.toggleShowConsole();
            DrawableTintTextView tvConsole = (DrawableTintTextView) v;
            tvConsole.setTint(getResources().getColor(tag ? R.color.colorPrimary : R.color.google_black_400));
            showVConsole(tag);
        } else if (id == R.id.tv_refresh_timing) {
            boolean tag = !refreshTiming;
            AppConfig.toggleRefreshTiming();
            DrawableTintTextView tvRefreshTiming = (DrawableTintTextView) v;
            tvRefreshTiming.setTint(getResources().getColor(tag ? R.color.colorPrimary : R.color.google_black_400));
            ZToast.normal("定时刷新TODO");
        } else if (id == R.id.tv_edit_mode) {
            boolean tag = !mEditMode;
            if (tag) {
                page.evaluateJavaScript("javascript:document.body.contentEditable = 'true'; document.designMode='on'; void 0", null);
            } else {
                page.evaluateJavaScript("javascript:document.body.contentEditable = 'false'; document.designMode='off'; void 0", null);
            }
            AppConfig.toggleEditMode();
            DrawableTintTextView tvEditMode = (DrawableTintTextView) v;
            tvEditMode.setTint(getResources().getColor(tag ? R.color.colorPrimary : R.color.google_black_400));
        } else if (id == R.id.tv_resource) {
            PrintingController printingController = PrintingControllerImpl.getInstance();
            if (printingController != null && !printingController.isBusy()) {
                printingController.startPrint(new TabPrinter(page),
                        new PrintManagerDelegateImpl(_mActivity));
                RecordUserAction.record("MobileMenuPrint");
            }
        } else if (id == R.id.tv_log) {
            page.evaluateJavaScript("javascript:window.touchblock=!window.touchblock;setTimeout(function(){JsInterface.blocktoggle(window.touchblock)}, 100);", null);
        } else if (id == R.id.tv_see_html) {
            LoadUrlEvent.post("view-source:" + page.getUrl(), true);
        } else if (id == R.id.tv_translate) {

        } else if (id == R.id.tv_smart_no_img) {
            boolean tag = !smartNoImageMode;
            ContentUtils.setImagesEnabled(Profile.getLastUsedRegularProfile(), !tag);
            ThreadPool.postOnUIThread(() -> {
                WebContents webContents = page.getWebContents();
                if (webContents != null) {
                    webContents.notifyRendererPreferenceUpdate();
                }
            });


            AppConfig.toggleSmartNoImageMode();
            DrawableTintTextView tvSmartNoImg = (DrawableTintTextView) v;
            tvSmartNoImg.setTint(getResources().getColor(tag ? R.color.colorPrimary : R.color.google_black_400));
        } else if (id == R.id.tv_reader_mode) {
            String distillerUrl = DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                    ReaderModeManager.DOM_DISTILLER_SCHEME, page.getUrl().getSpec(), page.getTitle());

            if (page instanceof ArkTabImpl) {
                LoadUrlEvent.post(((ArkTabImpl) page).getPageInfo(),
                        distillerUrl, true, page.isIncognito());
            } else {
                LoadUrlEvent.post(distillerUrl, true);
            }
        }
        dismiss();
    }

    public void initVConsole() {
        String js = "/*开发者调试工具*/\n" +
                "javascript:" +
                "window.vConsole = new window.VConsole({\n" +
                "  defaultPlugins: ['system', 'network', 'element', 'storage'], // 可以在此设定要默认加载的面板\n" +
                "  maxLogNumber: 1000,\n" +
                "  // disableLogScrolling: true,\n" +
                "  onReady: function() {\n" +
                "    console.log('vConsole is ready.');\n" +
                "  },\n" +
                "  onClearLog: function() {\n" +
                "    console.log('on clearLog');\n" +
                "  }\n" +
                "});" +
                "console.info('欢迎使用 vConsole。vConsole 是一个由微信公众平台前端团队研发的 Web 前端开发者面板，可用于展示 console 日志，方便开发、调试。');";
//                "var div = document.querySelector('div.vc-switch');" +
//                "div.setAttribute('style', 'display:none;');";
        mPage.evaluateJavaScript(js, null);
    }

    public void showVConsole(boolean flag) {
        if (flag) {
            mPage.evaluateJavaScript("javascript:" +
                    "var div = document.querySelectorAll('div.vc-switch')[0];" +
                    "if (div) {" +
                    "   div.removeAttribute('style');" +
                    "}", null);
        } else {
            mPage.evaluateJavaScript("javascript:" +
                    "var div = document.querySelector('div.vc-switch');" +
                    "if (div) {" +
                    "   div.setAttribute('style', 'display:none;');" +
                    "}", null);
        }
    }

}

