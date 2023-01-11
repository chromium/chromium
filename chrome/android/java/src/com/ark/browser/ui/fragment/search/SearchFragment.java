package com.ark.browser.ui.fragment.search;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.URLUtil;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.core.view.ViewCompat;

import com.ark.browser.database.SearchHistoryManager;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.model.SearchHistory;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.dialog.SearchEngineSelectDialog;
import com.ark.browser.ui.fragment.download.DownloadMultiData;
import com.ark.browser.ui.recycler.BookmarkMultiData;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.ui.widget.TitleHeaderLayout;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.HttpUtils;
import com.ark.browser.utils.KeywordUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.DialogAnimator;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.base.BaseDialogFragment;
import com.zpj.fragmentation.dialog.enums.DialogAnimation;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.MultiData;
import com.zpj.recyclerview.MultiRecycler;
import com.zpj.recyclerview.SingleTypeMultiData;
import com.zpj.recyclerview.layouter.GridLayouter;
import com.zpj.recyclerview.layouter.HorizontalLayouter;
import com.zpj.recyclerview.layouter.VerticalLayouter;
import com.zpj.recyclerview.manager.MultiLayoutManager;
import com.zpj.statemanager.State;
import com.zpj.toast.ZToast;
import com.zpj.utils.KeyboardUtils;
import com.zpj.widget.toolbar.ZSearchBar;

import org.chromium.base.Log;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

public class SearchFragment extends BaseDialogFragment<SearchFragment>
        implements HistoryProvider.BrowsingHistoryObserver,
        View.OnClickListener,
        IEasy.OnItemClickListener<SearchHistory>,
        IEasy.OnItemLongClickListener<SearchHistory> {

    private static final String TAG = "SearchFragment";

    private ZSearchBar searchBar;


    private final StickHeaderMultiData stickHeader1 = new StickHeaderMultiData("搜索历史");
    private final StickHeaderMultiData stickHeader0 = new StickHeaderMultiData("标签页");
//    private final StickHeaderMultiData stickHeader3 = new StickHeaderMultiData("主页图标");
    private final StickHeaderMultiData stickHeader4 = new StickHeaderMultiData("下载文件");
    private final StickHeaderMultiData stickHeader2 = new StickHeaderMultiData("最近浏览") {

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<Void> list, int position, List<Object> payloads) {
            super.onBindViewHolder(holder, list, position, payloads);
            holder.setVisible(R.id.clear_search_history, false);
        }
    };
    private final FlowHeaderMultiData flowHeaderMultiData = new FlowHeaderMultiData();
    private final TabListMultiData tabListMultiData = new TabListMultiData();
//    private final FavoriteItemMultiData favoriteItemMultiData = new FavoriteItemMultiData();
    private final DownloadMultiData downloadMultiData = new DownloadMultiData();
    private final GridHeaderMultiData gridHeaderMultiData = new GridHeaderMultiData();
    private final BookmarkMultiData bookmarkMultiData = new BookmarkMultiData(null);
    private final List<MultiData<?>> multiDataList = new ArrayList<MultiData<?>>() {
        {
            add(stickHeader1);
            add(flowHeaderMultiData);
            add(stickHeader0);
            add(tabListMultiData);
//            add(stickHeader3);
//            add(favoriteItemMultiData);
            add(stickHeader4);
            add(downloadMultiData);
            add(bookmarkMultiData);
            add(stickHeader2);
            add(gridHeaderMultiData);
        }
    };

    private MultiRecycler mRecycler;

    private TextView titleTextView;
    private TextView contentTextView;
    private ImageButton webBtn;
    private ImageButton searchBtn;
    private ImageButton copyBtn;
    private ImageButton editBtn;

    private HistoryProvider historyProvider;
    private String keyword = "";
    private boolean enterAnimEnd = false;

    public SearchFragment() {
        setMaxHeight(MATCH_PARENT);
        setDialogBackgroundColor(Color.TRANSPARENT);
    }

    public class StickHeaderMultiData extends SingleTypeMultiData<Void> {

        private String title;

        public StickHeaderMultiData(String title) {
            super(new VerticalLayouter());
            setState(State.STATE_CONTENT);
            hasMore = false;
            this.title = title;
        }

        public void setTitle(String title) {
            this.title = title;
        }

        @Override
        public boolean isStickyPosition(int position) {
            return true;
        }

        @Override
        public int getCount() {
            return 1;
        }

        @Override
        public int getLayoutId() {
            return R.layout.layout_header_title;
        }

        @Override
        public boolean loadData() {
            return false;
        }

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<Void> list, int position, List<Object> payloads) {
            TitleHeaderLayout headerLayout = holder.getView(R.id.bar_title);
            headerLayout.setTitle(title);
            ViewCompat.setElevation(holder.getItemView(), 0);
            holder.getItemView().setBackgroundColor(Color.TRANSPARENT);

            holder.setVisible(R.id.clear_search_history, true);
            holder.setOnClickListener(R.id.clear_search_history, new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    ZDialog.arrowMenu()
                            .setOrientation(LinearLayout.HORIZONTAL)
                            .setOptionMenus("全部清除")
                            .setOnItemClickListener((position, menu) -> {
                                ZToast.normal(menu.getTitle().toString());
                                ThreadPool.executeIO(() -> {
                                    SearchHistoryManager.deleteAllLocalSearchHistory();
                                    ThreadPool.postOnUIThread(SearchFragment.this::initFlowLayout);
                                });
                            })
                            .setAttachView(v)
                            .show(holder.getContext());
                }
            });
        }

        @Override
        public void onItemSticky(EasyViewHolder holder, int position, boolean isSticky) {
            super.onItemSticky(holder, position, isSticky);
            ViewCompat.setElevation(holder.getItemView(), isSticky ? 10 : 0);
            holder.getItemView().setBackgroundColor(isSticky ? Color.parseColor(AppConfig.isNightMode() ? "#e0000000" : "#f0ffffff") : Color.TRANSPARENT);
        }

    }

    private class GridHeaderMultiData extends SingleTypeMultiData<HistoryItem> {


        public GridHeaderMultiData() {
            super(new GridLayouter(2));
        }

        public void setData(List<HistoryItem> dataSet) {
            this.mData.clear();
            this.mData.addAll(dataSet);
            if (this.mData.isEmpty()) {
                setState(State.STATE_EMPTY);
            } else {
                setState(State.STATE_CONTENT);
            }
        }

        public HistoryItem removeAt(int index) {
            return this.mData.remove(index);
        }

        @Override
        public int getLayoutId() {
            return R.layout.item_search;
        }

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<HistoryItem> list, int position, List<Object> payloads) {
            TextView title = holder.getView(R.id.item_title);
            TextView url = holder.getView(R.id.item_url);
            HistoryItem item = list.get(position);
            url.setText(KeywordUtil.hightlight(Color.RED, item.getUrl().getSpec(), keyword));
            title.setText(KeywordUtil.hightlight(Color.RED, item.getTitle(), keyword));
            holder.setOnClickListener(R.id.refine_btn, v -> {
                searchBar.setText(item.getUrl().getSpec());
                searchBar.selectAll();
                contentTextView.setText(item.getUrl().getSpec());
                titleTextView.setText(item.getTitle());
            });
            holder.setOnItemClickListener(v -> openUrl(item.getUrl().getSpec()));
        }

        @Override
        public boolean loadData() {
            return false;
        }
    }

    private class TabListMultiData extends SingleTypeMultiData<ITab> {


        public TabListMultiData() {
            super(new HorizontalLayouter());
        }

        public void setData(List<ITab> dataSet) {
            this.mData.clear();
            this.mData.addAll(dataSet);
            if (this.mData.isEmpty()) {
                setState(State.STATE_EMPTY);
            } else {
                setState(State.STATE_CONTENT);
            }
        }

        @Override
        public int getLayoutId() {
            return R.layout.item_tab_card;
        }

        @Override
        public void onBindViewHolder(EasyViewHolder holder, List<ITab> list, int position, List<Object> payloads) {
            ITab tab = list.get(position);
            TextView tvTitle = holder.getView(R.id.tv_title);

            CardView cardView = holder.getView(R.id.card_view);
            FitWidthImageView ivThumbnail = holder.getView(R.id.iv_thumbnail);
            PageInfo pageInfo = tab.getCurrentPageInfo();
            if (pageInfo != null) {
                cardView.setCardBackgroundColor(pageInfo.getThemeColor());
                tvTitle.setText(pageInfo.getTitle());
                PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
            } else {
                cardView.setCardBackgroundColor(Color.WHITE);
                tvTitle.setText(null);
                ivThumbnail.setImageBitmap(null);
            }
            holder.setOnItemClickListener(v -> {
                TabListManager.getInstance().selectTab(tab);
                dismiss();
//                GetLauncherEvent.post(fragment -> {
//                    fragment.goToBrowser(false);
//                });
            });
        }

        @Override
        public boolean loadData() {
            return false;
        }
    }

//    private class FavoriteItemMultiData extends SingleTypeMultiData<FavoriteItem> {
//
//
//        public FavoriteItemMultiData() {
//            super(new HorizontalLayouter());
//        }
//
//        public void setData(List<FavoriteItem> dataSet) {
//            this.mData.clear();
//            this.mData.addAll(dataSet);
//            if (this.mData.isEmpty()) {
//                setState(State.STATE_EMPTY);
//            } else {
//                setState(State.STATE_CONTENT);
//            }
//        }
//
//        @Override
//        public int getLayoutId() {
//            return R.layout.item_app;
//        }
//
//        @Override
//        public void onBindViewHolder(EasyViewHolder holder, List<FavoriteItem> list, int position, List<Object> payloads) {
//            FavoriteItem item = list.get(position);
//            holder.setText(R.id.tv_title, item.getTitle());
//
//            FaviconUtil.with(holder.getContext(), item.getUrl())
//                    .setIconSize(ScreenUtils.dp2pxInt(56))
//                    .setTextSize(18)
//                    .setCallback(result -> holder.setImageDrawable(R.id.iv_icon, result))
//                    .start();
//
//            holder.setOnItemClickListener(v -> {
//                TabListManager.getInstance().openNewTab(new LoadUrlParams(item.getUrl()),
//                        TabLaunchType.FROM_CHROME_UI);
//                dismiss();
////                GetLauncherEvent.post(fragment -> {
////                    fragment.goToBrowser(false);
////                });
//            });
//        }
//
//        @Override
//        public boolean loadData() {
//            return false;
//        }
//    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_search;
    }

    @Override
    protected DialogAnimator onCreateDialogAnimator(ViewGroup contentView) {
        return new VerticalTranslateAnimator(contentView, DialogAnimation.TranslateFromBottom);
    }

    @Override
    protected DialogAnimator onCreateShadowAnimator(FrameLayout flContainer) {
        return super.onCreateShadowAnimator(flContainer);
    }

    @Override
    protected void initLayoutParams(ViewGroup view) {
        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) view.getLayoutParams();
        layoutParams.height = MATCH_PARENT;
        layoutParams.width = MATCH_PARENT;
        view.setFocusableInTouchMode(true);
        view.setFocusable(true);
        view.setClickable(true);
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        titleTextView = findViewById(R.id.text_title);
        contentTextView = findViewById(R.id.text_content);
        webBtn = findViewById(R.id.btn_web);
        webBtn.setOnClickListener(this);
        searchBtn = findViewById(R.id.btn_search);
        searchBtn.setOnClickListener(this);
        copyBtn = findViewById(R.id.btn_copy);
        copyBtn.setOnClickListener(this);
        editBtn = findViewById(R.id.btn_edit);
        editBtn.setOnClickListener(this);
        resetToolbar();

        searchBar = findViewById(R.id.search_bar);
        searchBar.setOnSearchListener(new ZSearchBar.OnSearchListener() {
            @Override
            public void onSearch(String keyword) {
                handleSearch(keyword);
            }
        });


        searchBar.setOnLeftButtonClickListener(v -> {
            KeyboardUtils.hideSoftInputKeyboard(searchBar.getEditor());
            new SearchEngineSelectDialog()
                    .onSingleSelect((fragment, position, item) -> {
                        if (!TemplateUrlServiceFactory.get().isLoaded()) {
                            TemplateUrlServiceFactory.get().load();
                        }
                        TemplateUrlServiceFactory.get().setSearchEngine(item.getKeyword());
                        searchBar.setLeftButtonImage(fragment.getSearchEngineIconRes(item));
                    })
                    .show(context);
        });

        searchBar.addTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                Log.d(TAG, "onTextChanged  isUrl2=" + UrlUtilities.isUrl(s.toString())
                        + "  isValidUrl=" + URLUtil.isValidUrl(s.toString()));
                Log.d(TAG, "onTextChange s=" + s + " enterAnimEnd=" + enterAnimEnd);
//                PageInfo pageInfo = TabListManager.getInstance().getCurrentPageInfo();
//                String url = pageInfo == null
//                        ? UrlConstantd.NTP_URL
//                        : pageInfo.getUrl();
//
//                mAutocomplete.start(profile, url, s.toString(), searchBar.getEditor().getSelectionStart(),
//                        false, true);
                if (s.toString().isEmpty() && enterAnimEnd) {
                    stickHeader1.setTitle("搜索历史");
                    initFlowLayout();
                    keyword = "";
                    historyProvider.queryHistory("");
                    resetToolbar();
                } else {
                    contentTextView.setText(s);
                    titleTextView.setText(s);
                    keywordFilter(s.toString());
                }

                searchTabInfo();

//                searchFavoriteItem();

                searchDownloadItem();

                bookmarkMultiData.updateKeyword(keyword);

            }

            @Override
            public void afterTextChanged(Editable s) {

            }
        });

        mRecycler = new MultiRecycler(findViewById(R.id.recycler_view), multiDataList)
                .setLayoutManager(new MultiLayoutManager())
                .build();


        init();


        post(() -> showSoftInput(searchBar.getEditor()));
    }

    @Override
    public void onDestroyView() {
        hideSoftInput();
        super.onDestroyView();
    }

    public void onCreate() {

        historyProvider = new BrowsingHistoryBridge(Profile.getLastUsedRegularProfile());
        historyProvider.setObserver(this);
        enterAnimEnd = false;
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {

        gridHeaderMultiData.setData(items);

        Log.d(TAG, "onQueryHistoryComplete items=" + items);
        mRecycler.notifyDataSetChanged();
    }

    @Override
    public void onHistoryDeleted() {

    }

    @Override
    public void onRemoveComplete() {

    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {

    }

    @Override
    public void onClick(View v) {
//        if (v == search_forward) {
//            String keyword = search_input.getText().toString();
//            if (!keyword.isEmpty() && !keyword.trim().isEmpty()) {
//                handleSearch(keyword.trim());
//            }
//        } else if (v == search_clear) {
//            search_input.setText("");
//        } else
        if (v == webBtn) {
            openUrl(contentTextView.getText().toString());
        } else if (v == searchBtn) {
            search(contentTextView.getText().toString());
        } else if (v == copyBtn) {
            Clipboard.getInstance().setTextFromUser(contentTextView.getText().toString());
        } else if (v == editBtn) {
            CharSequence charSequence = contentTextView.getText();
            searchBar.setText(charSequence);
            searchBar.selectAll();
//            if (charSequence.length() != 0) {
//                search_input.setSelection(charSequence.length());
//            }
        }
    }


    public void resetToolbar() {
        PageInfo pageInfo = TabListManager.getInstance().getCurrentPageInfo();
        if (pageInfo != null) {
            titleTextView.setText(pageInfo.getTitle());
            contentTextView.setText(pageInfo.getUrl());
        } else {
            titleTextView.setText("来自剪贴板");
            contentTextView.setText(Clipboard.getInstance().getFirstText());
        }
    }

    private void initFlowLayout(){
        flowHeaderMultiData.setOnItemClickListener(this);
        flowHeaderMultiData.setOnItemLongClickListener(this);

        ThreadPool.executeIO(() -> {
            List<SearchHistory> historyList = SearchHistoryManager.getSearchHistoryLimited(10);
            ThreadPool.runOnUIThread(() -> {
                flowHeaderMultiData.setData(historyList);
                mRecycler.notifyDataSetChanged();
            });
        });
    }

    private void handleSearch(String query){
        KeyboardUtils.hideSoftInputKeyboard(searchBar.getEditor());
        insertToDB(query);



        if (UrlUtilities.isUrl(query)) {
            query = query;
        } else {

            if (!TemplateUrlServiceFactory.get().isLoaded()) {
                TemplateUrlServiceFactory.get().load();
            }
            query = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query);
        }
        dismiss();
        LoadUrlEvent.post(query);
    }

    private void search(String text) {
        insertToDB(text);
        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            TemplateUrlServiceFactory.get().load();
        }
        text = TemplateUrlServiceFactory.get().getUrlForSearchQuery(text);
        dismiss();
        LoadUrlEvent.post(text);
    }

    private void openUrl(String url) {
        insertToDB(url);
        dismiss();
        LoadUrlEvent.post(url);
    }

    private void insertToDB(String query) {
        ThreadPool.executeIO(() -> {
//            SearchHistory history = SearchHistoryManager.getSearchHistoryByText(query);
//            if (history == null) {
//                history = new SearchHistory();
//                history.setText(query);
//            }
//            history.setTime(System.currentTimeMillis());

            SearchHistoryManager.saveSearchHistory(query);
            ThreadPool.runOnUIThread(this::initFlowLayout);
        });

        historyProvider.queryHistory("");
    }

    private void keywordFilter(String s){
        if (s.isEmpty()) {
            return;
        }
        keyword = s;
        Log.d(TAG, "keywordFilter keyword=" + keyword);
        historyProvider.queryHistory(s);

        String url = String.format(Locale.ENGLISH,
                "https://www.baidu.com/sugrec?pre=1&p=3&ie=utf-8&json=1&prod=pc&from=pc_web&wd=%s&req=2&csor=2&cb=&_=%d",
                s, System.currentTimeMillis());
        // "http://suggestion.baidu.com/su?wd=" + s
        HttpUtils.get(url, new HttpUtils.Callback() {
            @Override
            public void onFailed(Exception e) {
                e.printStackTrace();
            }

            @Override
            public void onSuccess(String body) {
                Log.d(TAG, "body111=" + body);



                List<String> strings = new ArrayList<>();
                try {
                    body = body.substring(1, body.length() - 1);
                    JSONObject object = new JSONObject(body);
                    if (object.has("g")) {
                        JSONArray array = object.getJSONArray("g");
                        for (int i = 0; i < array.length(); i++) {
                            JSONObject obj = array.getJSONObject(i);
                            if (obj.has("q")) {
                                strings.add(obj.getString("q"));
                            }
                        }
                    } else {
                        strings.add(s);
                    }
                } catch (JSONException e) {
                    e.printStackTrace();
                }

//                if (body.contains("s:[]")) {
//                    body = s;
//                } else {
//                    body = body.substring(body.indexOf("s:[\"") + 3, body.indexOf("]});"));
//                }
//
//                final String[] strings = body.replace("\"", "").split(",");
//                Log.d(TAG, "body222=" + body);
//                Log.d(TAG, "strings=" + Arrays.toString(strings));

                List<SearchHistory> list = new ArrayList<>();
                for (String keyword : strings) {
                    SearchHistory history = new SearchHistory();
                    history.setText(keyword);
                    list.add(history);
                }
                stickHeader1.setTitle("关键词联想");
                flowHeaderMultiData.setData(list);
                flowHeaderMultiData.setOnItemLongClickListener(null);
                flowHeaderMultiData.setOnItemClickListener((index, v, history) -> {
                    searchBar.setText(history.getText());
                    searchBar.selectAll();
                });
                mRecycler.notifyDataSetChanged();
            }
        });
    }

    @Override
    public void onClick(EasyViewHolder holder, View v, SearchHistory history) {
        handleSearch(history.getText());
    }

    @Override
    public boolean onLongClick(EasyViewHolder holder, View v, SearchHistory history) {
        ZDialog.arrowMenu()
                .setOrientation(LinearLayout.HORIZONTAL)
                .setOptionMenus("复制", "删除")
                .setOnItemClickListener((position, menu) -> {
                    ZToast.normal(menu.getTitle().toString());
                    switch (position) {
                        case 0:
                            Clipboard.getInstance().setTextFromUser(history.getText());
                            break;
                        case 1:
                            flowHeaderMultiData.removeAt(holder.getAdapterPosition());
                            ThreadPool.executeIO(() -> SearchHistoryManager.deleteSearchHistoryByText(history.getText()));
                            break;
                    }
                })
                .setAttachView(v)
                .show(v.getContext());
        return true;
    }

    public void init() {
        onCreate();
        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            TemplateUrlServiceFactory.get().load();
        }
        TemplateUrl defaultSearchEngine = TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();

        if (defaultSearchEngine != null) {
            searchBar.getLeftImageButton().setImageResource(SearchEngineSelectDialog.getSearchEngineIconRes(defaultSearchEngine));
        }
        long time1 = System.currentTimeMillis();
        initFlowLayout();
        keyword = "";
        historyProvider.queryHistory("");
        resetToolbar();

//        searchBar.getEditor().setOnItemClickListener(new AdapterView.OnItemClickListener() {
//            @Override
//            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
//                String url = ((TextView) view.findViewById(R.id.record_item_url)).getText().toString();
//                searchBar.getEditor().setText(url);
//            }
//        });
        enterAnimEnd = true;
        long time2 = System.currentTimeMillis();
        Log.d(TAG, "deltaTime=" + (time2 - time1));

//        tabListMultiData.setData(TabListManager.getInstance().getCurrentTabList().getTabInfoList());
//        stickHeader0.setTitle("标签页(" + tabListMultiData.getCount() + ")");

        searchTabInfo();

//        searchFavoriteItem();

        searchDownloadItem();

    }

    private void searchTabInfo() {
        ThreadPool.execute(() -> {
            List<ITab> all = TabListManager.getInstance().getCurrentTabList().getTabList();
            List<ITab> tabList = new ArrayList<>();
            if (TextUtils.isEmpty(keyword)) {
                tabList.addAll(all);
            } else {
                for (ITab tab : all) {
                    PageInfo info = tab.getCurrentPageInfo();
                    if (info != null) {
                        String key = keyword.toLowerCase();
                        if (info.getTitle().toLowerCase().contains(key)
                                || info.getUrl().toLowerCase().contains(key)) {
                            tabList.add(tab);
                        }
                    }
                }
            }
            Collections.sort(tabList, new Comparator<ITab>() {
                @Override
                public int compare(ITab o1, ITab o2) {
                    return Long.compare(o2.getTabInfo().getAccessTime(), o1.getTabInfo().getAccessTime());
                }
            });
            ThreadPool.runOnUIThread(() -> {
                tabListMultiData.setData(tabList);
                stickHeader0.setTitle("标签页(" + tabListMultiData.getCount() + ")");
                tabListMultiData.notifyDataSetChange();
            });
        });
    }

//    private void searchFavoriteItem() {
//        ThreadPool.execute(() -> {
//            List<FavoriteItem> list = HomepageManager.getAppFavorites();
//            if (!TextUtils.isEmpty(keyword)) {
//                Iterator<FavoriteItem> it = list.iterator();
//                while (it.hasNext()) {
//                    FavoriteItem item = it.next();
//                    if (!item.getTitle().toLowerCase().contains(keyword)
//                            && !item.getUrl().toLowerCase().contains(keyword)) {
//                        it.remove();
//                    }
//                }
//            }
//            ThreadPool.post(() -> {
//                favoriteItemMultiData.setData(list);
//                stickHeader3.setTitle("主页图标(" + favoriteItemMultiData.getCount() + ")");
//                favoriteItemMultiData.notifyDataSetChange();
//            });
//        });
//    }

    private void searchDownloadItem() {
        DownloadManagerService downloadManagerService = DownloadManagerService.getDownloadManagerService();
        DownloadManagerService.DownloadObserver downloadObserver = new DownloadManagerService.DownloadObserver() {
            @Override
            public void onAllDownloadsRetrieved(List<DownloadItem> list, ProfileKey profileKey) {

                ArkLogger.e(this, "onAllDownloadsRetrieved size=" + list.size());


                downloadManagerService.removeDownloadObserver(this);

                List<DownloadItem> items = new ArrayList<>(list);
                ThreadPool.execute(() -> {

                    if (!TextUtils.isEmpty(keyword)) {
                        Iterator<DownloadItem> it = items.iterator();
                        while (it.hasNext()) {
                            DownloadInfo item = it.next().getDownloadInfo();
                            if (!item.getFileName().toLowerCase().contains(keyword)
                                    && !item.getFilePath().toLowerCase().contains(keyword)
                                    && !item.getUrl().getSpec().toLowerCase().contains(keyword)
                                    && !item.getOriginalUrl().toLowerCase().contains(keyword)) {
                                it.remove();
                            }
                        }
                    }
                    ThreadPool.runOnUIThread(() -> {
                        downloadMultiData.setData(items);
                        stickHeader4.setTitle("下载文件(" + downloadMultiData.getCount() + ")");
                        downloadMultiData.notifyDataSetChange();
                    });
                });

            }

            @Override
            public void onDownloadItemCreated(DownloadItem item) {

            }

            @Override
            public void onDownloadItemUpdated(DownloadItem item) {

            }

            @Override
            public void onDownloadItemFinished(DownloadItem item) {

            }

            @Override
            public void onDownloadItemRemoved(String guid) {

            }

            @Override
            public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {

            }
        };
        downloadManagerService.addDownloadObserver(downloadObserver);
        downloadManagerService.getAllDownloads(null);

    }

}

