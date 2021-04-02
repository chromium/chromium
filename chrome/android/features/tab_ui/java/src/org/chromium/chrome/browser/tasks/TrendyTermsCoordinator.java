// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TrendyTermsCoordinator.ItemType.TRENDY_TERMS;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_ICON_DRAWABLE_ID;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_ICON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_STRING;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Coordinator that manages trendy terms in start surface.
 */
public class TrendyTermsCoordinator {
    @IntDef({TRENDY_TERMS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        int TRENDY_TERMS = 0;
    }

    private final MVCListAdapter.ModelList mListItems;
    private final Supplier<Tab> mParentTabSupplier;

    TrendyTermsCoordinator(
            Context context, RecyclerView recyclerView, Supplier<Tab> parentTabSupplier) {
        mListItems = new MVCListAdapter.ModelList();
        mParentTabSupplier = parentTabSupplier;
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mListItems);
        adapter.registerType(TRENDY_TERMS, parent -> {
            ViewLookupCachingFrameLayout view =
                    (ViewLookupCachingFrameLayout) LayoutInflater.from(context).inflate(
                            R.layout.trendy_terms_item, parent, false);
            view.setClickable(true);
            return view;
        }, TrendyTermsViewBinder::bind);
        recyclerView.setAdapter(adapter);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
    }

    @VisibleForTesting
    void addTrendyTermForTesting(@ItemType int type, PropertyModel model) {
        mListItems.add(new SimpleRecyclerViewAdapter.ListItem(type, model));
    }

    public void populateTrendyTerms() {
        PostTask.postTask(TaskTraits.THREAD_POOL_USER_VISIBLE, () -> {
            List<String> terms = TrendyTermsCache.getTrendyTerms();
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { updateTrendyTerms(terms); });
        });
    }

    private void updateTrendyTerms(List<String> terms) {
        if (!needsUpdate(terms)) return;

        mListItems.clear();
        for (String trendyTerm : terms) {
            OnClickListener listener = v -> {
                RecordUserAction.record("StartSurface.TrendyTerms.TapTerm");
                String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(trendyTerm);
                ReturnToChromeExperimentsUtil.handleLoadUrlFromStartSurface(
                        new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK), null /*incognito*/,
                        mParentTabSupplier.get());
            };
            PropertyModel trendInfo =
                    new PropertyModel.Builder(TrendyTermsProperties.ALL_KEYS)
                            .with(TRENDY_TERM_STRING, trendyTerm)
                            .with(TRENDY_TERM_ICON_DRAWABLE_ID, R.drawable.ic_search)
                            .with(TRENDY_TERM_ICON_ON_CLICK_LISTENER, listener)
                            .build();
            mListItems.add(new SimpleRecyclerViewAdapter.ListItem(TRENDY_TERMS, trendInfo));
        }
    }

    private boolean needsUpdate(List<String> terms) {
        if (terms.size() != mListItems.size()) return true;
        for (int i = 0; i < terms.size(); i++) {
            if (!terms.get(i).equals(mListItems.get(i).model.get(TRENDY_TERM_STRING))) return true;
        }
        return false;
    }
}
