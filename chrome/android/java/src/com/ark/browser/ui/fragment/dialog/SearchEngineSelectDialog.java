package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import com.zpj.fragmentation.dialog.impl.BottomSelectDialogFragment;

import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.chrome.R;

import java.util.List;

public class SearchEngineSelectDialog extends BottomSelectDialogFragment<TemplateUrl, SearchEngineSelectDialog> {

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_layout_center_impl_list;
    }

    public SearchEngineSelectDialog() {
        setTitle("搜索引擎");

        TemplateUrlServiceFactory.get().load();
        List<TemplateUrl> items = TemplateUrlServiceFactory.get().getTemplateUrls();
        setData(items);

        TemplateUrl defaultTemplateUrl = TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
        int select = 0;
        for (select = 0; select < items.size(); select++) {
            if (TextUtils.equals(items.get(select).getKeyword(), defaultTemplateUrl.getKeyword())) {
                break;
            }
        }

        setSelected(select);
        onBindTitle((titleView, item, position) -> titleView.setText(item.getShortName()));
        onBindIcon((icon, item, position) -> icon.setImageResource(getSearchEngineIconRes(item)));
        onBindSubtitle((subtitleView, item, position) -> subtitleView.setText(item.getKeyword()));
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        findViewById(R.id.btn_close).setOnClickListener(v -> dismiss());
    }

    public int getSearchEngineIconRes(TemplateUrl templateUrl) {
        switch (templateUrl.getKeyword()) {
            case "google.com":
                return R.drawable.ic_browser_engine_google;
            case "bing.com":
                return R.drawable.ic_browser_engine_bing;
            case "sogou.com":
                return R.drawable.ic_browser_engine_sougou;
            case "so.com":
                return R.drawable.ic_browser_engine_360;
            case "baidu.com":
            default:
                return R.drawable.ic_browser_engine_baidu;
        }
    }

    public int getSearchEngineLogoRes(TemplateUrl templateUrl) {
        switch (templateUrl.getKeyword()) {
            case "google.com":
                return R.drawable.ic_browser_engine_google_logo;
            case "bing.com":
                return R.drawable.ic_browser_engine_bing_logo;
            case "sogou.com":
                return R.drawable.ic_browser_engine_sougou_logo;
            case "so.com":
                return R.drawable.ic_browser_engine_360_logo;
            case "baidu.com":
            default:
                return R.drawable.ic_browser_engine_baidu_logo;
        }
    }

}