package com.ark.browser.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.database.SearchHistoryManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.skin.SkinEngine;
import com.zpj.toast.ZToast;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsStateBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

import java.util.List;

public class RedirectChainView extends LinearLayout {

//    public interface Callback {
//
//        void onOpenUrl(GURL url);
//
//
//    }
//
//    private Callback mCallback;

    private ITab mTab;

    public RedirectChainView(Context context) {
        this(context, null);
    }

    public RedirectChainView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public RedirectChainView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setOrientation(LinearLayout.VERTICAL);
    }

//    public void setCallback(Callback callback) {
//        mCallback = callback;
//    }

    public void setPageInfo(ITab tab, PageInfo pageInfo) {
        mTab = tab;
        removeAllViews();
        ArkWebContents arkWb = ArkWebManager.get(pageInfo.getId());
        if (arkWb != null && !arkWb.isDestroyed()) {
            NavigationHistory history = arkWb.getNavigationController().getNavigationHistory();
            for (int i = history.getEntryCount() - 1; i >= 0; i--) {
                NavigationEntry entry = history.getEntryAtIndex(i);
                if (entry.getRedirectChain() != null) {
                    for (int j = entry.getRedirectChain().size() - 1; j >= 0; j--) {
                        GURL url = entry.getRedirectChain().get(j);
                        if (url.equals(entry.getUrl())) {
                            addEntryView(entry.getUrl());
                        } else {
                            addChainView(url);
                        }
                    }
                } else {
                    addEntryView(entry.getUrl());
                }
            }
        } else {
            // TODO optimise
            long start = System.currentTimeMillis();
            TabState tabState = ArkTabDao.restorePageState(pageInfo.pageId);
            if (tabState != null && tabState.contentsState != null) {
                ArkLogger.e(this, "setPageInfo restorePageState deltaTime=" + (System.currentTimeMillis() - start));


                for (List<GURL> urls : WebContentsStateBridge.getRedirectChainFromState(tabState.contentsState)) {
                    for (int j = urls.size() - 1; j >= 0; j--) {
                        GURL url = urls.get(j);
                        if (j == urls.size() - 1) {
                            addEntryView(url);
                        } else {
                            addChainView(url);
                        }
                    }
                }

//                WebContents webContents = WebContentsStateBridge.restoreContentsFromByteBuffer(
//                        tabState.contentsState, true, true);
//                NavigationHistory history = webContents.getNavigationController().getNavigationHistory();
//                for (int i = history.getEntryCount() - 1; i >= 0; i--) {
//                    NavigationEntry entry = history.getEntryAtIndex(i);
//                    if (entry.getRedirectChain() != null) {
//                        for (int j = entry.getRedirectChain().size() - 1; j >= 0; j--) {
//                            GURL url = entry.getRedirectChain().get(j);
//                            if (url.equals(entry.getUrl())) {
//                                addEntryView(entry.getUrl());
//                            } else {
//                                addChainView(url);
//                            }
//                        }
//                    } else {
//                        addEntryView(entry.getUrl());
//                    }
//                }
                ArkLogger.e(this, "setPageInfo deltaTime=" + (System.currentTimeMillis() - start));
            } else {
                addEntryView(new GURL(pageInfo.getUrl()));
            }
        }
    }

    private View addEntryView(GURL url) {
        View chainUrl = createUrlView(url);
        TextView tvUrl = chainUrl.findViewById(R.id.tv_url);
        tvUrl.setText(url.getSpec());
        View handle = chainUrl.findViewById(R.id.handle);
        handle.setVisibility(GONE);
        SkinEngine.setTextColor(tvUrl, R.attr.colorPrimary);
        addView(chainUrl);
        return chainUrl;
    }

    private View addChainView(GURL url) {
        View chainUrl = createUrlView(url);
        TextView tvUrl = chainUrl.findViewById(R.id.tv_url);
        tvUrl.setText(url.getSpec());
        addView(chainUrl);
        return chainUrl;
    }

    private View createUrlView(GURL url) {
        View chainUrl = LayoutInflater.from(getContext()).inflate(R.layout.item_history_stack_url, null, false);
        chainUrl.setOnLongClickListener(new OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                ZDialog.arrowMenu()
                        .setOrientation(LinearLayout.HORIZONTAL)
                        .setOptionMenus("复制", "新标签中打开")
                        .setOnItemClickListener((position, menu) -> {
                            ZToast.normal(menu.getTitle().toString());
                            switch (position) {
                                case 0:
                                    Clipboard.getInstance().setTextFromUser(url.getSpec());
                                    ZToast.success("链接已复制到剪切板");
                                    break;
                                case 1:
//                                    if (mCallback != null) {
//                                        mCallback.onOpenUrl(url);
//                                    }
                                    if (mTab != null) {
                                        LoadUrlParams params = new LoadUrlParams(url);
                                        mTab.openNewTab(params, TabLaunchType.FROM_CHROME_UI);
                                    }
                                    break;
                            }
                        })
                        .setAttachView(view)
                        .show(getContext());
                return true;
            }
        });
        return chainUrl;
    }

}
