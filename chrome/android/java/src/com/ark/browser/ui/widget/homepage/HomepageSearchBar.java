package com.ark.browser.ui.widget.homepage;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import com.ark.browser.ui.fragment.dialog.SearchEngineSelectDialog;
import com.ark.browser.ui.fragment.search.SearchFragment;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.UiThreadTaskTraits;

public class HomepageSearchBar extends FrameLayout implements TemplateUrlService.TemplateUrlServiceObserver {

    private ImageView iconView;
    private ImageView searchView;
    private TextView textView;
    private ImageView logoView;

    public HomepageSearchBar(Context context) {
        this(context, null);
    }

    public HomepageSearchBar(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public HomepageSearchBar(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
        ThreadPool.postOnUIThread(() -> {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    TemplateUrlServiceFactory.get().addObserver(HomepageSearchBar.this);
                }
            });
        });
    }

    @Override
    public void setPadding(int left, int top, int right, int bottom) {
        super.setPadding(0, 0, 0, 0);
    }

    private void init(Context context) {
        View v = LayoutInflater.from(context)
                .inflate(R.layout.layout_search_bar, this, true);
        iconView = v.findViewById(R.id.btn_qsb_icon);
        searchView = v.findViewById(R.id.btn_qsb_search);
        textView = v.findViewById(R.id.btn_qsb_text);
        logoView = v.findViewById(R.id.img_logo);
        searchView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                new SearchFragment().show(context);
            }
        });
        iconView.setOnClickListener(v1 -> {
            new SearchEngineSelectDialog()
                    .onSingleSelect((fragment, position, item) -> {
                        TemplateUrlServiceFactory.get().setSearchEngine(item.getKeyword());
                        refreshSearchEngineLogo();
                    })
                    .show(context);
        });
        textView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                new SearchFragment().show(context);
            }
        });
    }

    @Override
    public void onTemplateURLServiceChanged() {
        if (TemplateUrlServiceFactory.get().isLoaded()) {
            refreshSearchEngineLogo();
        }
    }

    private void refreshSearchEngineLogo() {
        TemplateUrl url = TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
        if (url != null) {
            iconView.setImageResource(SearchEngineSelectDialog.getSearchEngineIconRes(url));
            logoView.setImageResource(SearchEngineSelectDialog.getSearchEngineLogoRes(url));
        }
    }
}
