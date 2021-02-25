// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.shopping;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.StaggeredGridLayoutManager;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.util.BitmapCache;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ChromeImageButton;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** The coordinator for managing the shopping surface in bookmarks. */
public class ShoppingCoordinator {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ItemMenuId.NEW_TAB, ItemMenuId.NEW_INCOGNITO_TAB, ItemMenuId.EDIT, ItemMenuId.MOVE_TO,
            ItemMenuId.DELETE})
    public @interface ItemMenuId {
        int NEW_TAB = 0;
        int NEW_INCOGNITO_TAB = 1;
        int EDIT = 2;
        int MOVE_TO = 3;
        int DELETE = 4;
    }

    public static final int RECYCLER_HEAD_ITEM = 0;
    public static final int RECYCLER_SHOPPING_ITEM = 1;

    private static final String ENDPOINT_URL =
            "https://task-management-chrome.sandbox.google.com/bookmarks/enrichment?locale=en:US";
    private static final String HTTPS_METHOD = "POST";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String[] SCOPES =
            new String[] {"https://www.googleapis.com/auth/userinfo.email",
                    "https://www.googleapis.com/auth/userinfo.profile"};
    private static final long ENDPOINT_TIMEOUT = 20000;

    private Context mContext;

    private ViewGroup mRootView;

    private ViewGroup mFilterSectionRoot;

    private BookmarkModel mBookmarkModel;

    private BookmarkId mShoppingRoot;

    private List<PropertyModel> mAllShoppingBookmarkModels;

    private ModelList mRecyclerModels;

    private ChipList mChipList;

    private ListItem mAllCategoryChip;
    private ListItem mOtherCategoryChip;

    private ModelList mChipModels;

    private ImageFetcher mImageFetcher;

    private Set<Long> mShoppingIds;

    private String mActiveQuery;

    private String mActiveCategory;

    private PrefixTree mTagTree;

    private final Callback<String> mOpenInCCTFunction;

    private final Map<Long, PropertyModel> mIdToModelMap;

    private final Map<String, Set<Long>> mCategoryMapping;

    private final Handler mHandler;
    private final Runnable mUpdateSearchRunnable;

    private final TabDelegate mRegularTabDelegate;
    private final TabDelegate mIncognitoTabDelegate;

    private BitmapCache mProductImageCache;

    private boolean mIsDestroyed;

    private Intent mStartupIntent;

    public ShoppingCoordinator(Context context, Runnable backDelegate, Runnable closeDelegate,
            Callback<String> CCTDelegate, BookmarkModel bookmarkModel, BookmarkId shoppingRoot,
            Intent intent) {
        mContext = context;
        mStartupIntent = intent;
        mBookmarkModel = bookmarkModel;
        mShoppingRoot = shoppingRoot;
        mOpenInCCTFunction = CCTDelegate;
        mRegularTabDelegate = new TabDelegate(false);
        mIncognitoTabDelegate = new TabDelegate(true);
        mIdToModelMap = new HashMap<>();
        mShoppingIds = new HashSet<>();
        mCategoryMapping = new HashMap<>();
        mTagTree = new PrefixTree();
        mHandler = new Handler();
        mRootView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.shopping_main, null);

        mProductImageCache = new BitmapCache(GlobalDiscardableReferencePool.getReferencePool(), 10);

        mUpdateSearchRunnable = () -> filterShoppingItems(mActiveQuery, mActiveCategory);

        mFilterSectionRoot = initializeFilterView(new SelectableListToolbar.SearchDelegate() {
            @Override
            public void onSearchTextChanged(String query) {
                mActiveQuery = query;
                mHandler.removeCallbacks(mUpdateSearchRunnable);
                mHandler.postDelayed(mUpdateSearchRunnable, 400);
            }

            @Override
            public void onEndSearch() {

            }
        }, R.string.shopping_search_placeholder);

        ChromeImageButton backButton = mRootView.findViewById(R.id.back);
        backButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                backDelegate.run();
            }
        });

        ChromeImageButton closeButton = mRootView.findViewById(R.id.close);
        closeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                closeDelegate.run();
            }
        });

        mBookmarkModel.addObserver(new BookmarkBridge.BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
            }

            @Override
            public void bookmarkNodeAdded(BookmarkBridge.BookmarkItem parent, int index) {
                BookmarkId newId = mBookmarkModel.getChildAt(parent.getId(), index);
                updateForAddition(newId);
                List<BookmarkId> ids = new ArrayList<>(1);
                ids.add(newId);
                filterShoppingItems(mActiveQuery, mActiveCategory);
                makeShoppingRequest(ids);
            }

            @Override
            public void bookmarkNodeMoved(BookmarkBridge.BookmarkItem oldParent, int oldIndex,
                    BookmarkBridge.BookmarkItem newParent, int newIndex) {
                BookmarkId movedItemId = mBookmarkModel.getChildAt(newParent.getId(), newIndex);
                if (newParent.getId().getId() == mShoppingRoot.getId()) {
                    updateForAddition(movedItemId);
                    List<BookmarkId> ids = new ArrayList<>(1);
                    ids.add(movedItemId);
                    filterShoppingItems(mActiveQuery, mActiveCategory);
                    makeShoppingRequest(ids);
                } else {
                    updateForRemoval(movedItemId);
                }
            }

            @Override
            public void bookmarkNodeRemoved(BookmarkBridge.BookmarkItem parent, int oldIndex,
                    BookmarkBridge.BookmarkItem node) {
                updateForRemoval(node.getId());
            }
        });

        initializeRecyclerView();
    }

    public View getView() {
        return mRootView;
    }

    private void initializeRecyclerView() {
        RecyclerView recycler = mRootView.findViewById(R.id.product_list);

        List<BookmarkId> bookmarks = mBookmarkModel.getChildIDs(mShoppingRoot);

        mRecyclerModels = new ModelList();
        mAllShoppingBookmarkModels = new ArrayList<>();

        Map<Long, JSONObject> cache = ShoppingCache.updateAndGetCache(null, bookmarks);
        List<BookmarkId> idsToUpdate = new ArrayList<>();
        for (BookmarkId id : bookmarks) {
            updateForAddition(id);
            if (cache.containsKey(id.getId())) {
                updateItemWithMeta(id.getId(), cache.get(id.getId()));

                // Only make an API request for items that need updating.
                if (ShoppingCache.itemNeedsUpdate(cache.get(id.getId()))) {
                    idsToUpdate.add(id);
                }
            } else {
                // If the item isn't in the cache, require an update.
                idsToUpdate.add(id);

                // Update icon for the model if possible
                updateModelWithFavicon(mIdToModelMap.get(id.getId()));
            }
        }

        mRecyclerModels.add(new ListItem(RECYCLER_HEAD_ITEM, new PropertyModel()));
        for (PropertyModel model : mAllShoppingBookmarkModels) {
            mRecyclerModels.add(new ListItem(RECYCLER_SHOPPING_ITEM, model));
        }

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mRecyclerModels);
        adapter.registerType(RECYCLER_HEAD_ITEM,
                new ViewBuilder<View>() {
                    @Override
                    public View buildView(ViewGroup parent) {
                        StaggeredGridLayoutManager.LayoutParams params =
                                new StaggeredGridLayoutManager.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.WRAP_CONTENT);
                        params.setFullSpan(true);
                        mFilterSectionRoot.setLayoutParams(params);
                        return mFilterSectionRoot;
                    }
                },
                ShoppingListItemProperties::bindListItem);

        adapter.registerType(RECYCLER_SHOPPING_ITEM,
                new LayoutViewBuilder(R.layout.shopping_list_item),
                ShoppingListItemProperties::bindListItem);

        recycler.setLayoutManager(
                new StaggeredGridLayoutManager(2, StaggeredGridLayoutManager.VERTICAL));
        recycler.setAdapter(adapter);

        FadingShadowView shadowView = mRootView.findViewById(R.id.fading_shadow);
        shadowView.init(
                ApiCompatibilityUtils.getColor(
                        mContext.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);
        recycler.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                boolean showShadow = recycler.canScrollVertically(-1);
                shadowView.setVisibility(showShadow ? View.VISIBLE : View.GONE);
            }
        });

        if (mStartupIntent != null) {
            String bookmarkIdString = mStartupIntent.getStringExtra(
                    ShoppingNotificationService.EXTRA_PRODUCT_BOOKMARK_ID);
            BookmarkId scrollToId = BookmarkId.getBookmarkIdFromString(bookmarkIdString);

            for (int i = 0; i < mAllShoppingBookmarkModels.size(); i++) {
                BookmarkId curId = BookmarkId.getBookmarkIdFromString(
                        mAllShoppingBookmarkModels.get(i).get(
                                ShoppingListItemProperties.ID_STRING));
                if (curId.getId() == scrollToId.getId()) {
                    recycler.scrollToPosition(i);
                    android.util.Log.w("mdjones", "Scrolled to item: " + i);
                    break;
                }
            }
        }

        makeShoppingRequest(idsToUpdate);
    }

    private void updateForAddition(BookmarkId id) {
        mShoppingIds.add(id.getId());
        BookmarkBridge.BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        PropertyModel model = new PropertyModel.Builder(ShoppingListItemProperties.ALL_KEYS)
                .with(ShoppingListItemProperties.ID, id.getId())
                .with(ShoppingListItemProperties.ID_STRING, id.toString())
                .with(ShoppingListItemProperties.TITLE, item.getTitle())
                .with(ShoppingListItemProperties.DOMAIN, item.getUrl().getHost())
                .with(ShoppingListItemProperties.PRICE, null)
                .with(ShoppingListItemProperties.CLICK_DELEGATE,
                        () -> mOpenInCCTFunction.onResult(item.getUrl().getSpec()))
                .with(ShoppingListItemProperties.IMAGE, null)
                .with(ShoppingListItemProperties.USING_FAVICON, false)
                .build();
        model.set(ShoppingListItemProperties.MENU_DELEGATE,
                createItemMenuButtonDelegate(id, model));
        mIdToModelMap.put(id.getId(), model);

        // Add to the front of the list, effectively sorting by newest.
        mAllShoppingBookmarkModels.add(0, model);
    }

    private void updateForRemoval(BookmarkId id) {
        for (int i = 0; i < mAllShoppingBookmarkModels.size(); i++) {
            if (mAllShoppingBookmarkModels.get(i).get(ShoppingListItemProperties.ID)
                    == id.getId()) {
                mAllShoppingBookmarkModels.remove(i);
                break;
            }
        }

        for (int i = 0; i < mRecyclerModels.size(); i++) {
            if (mRecyclerModels.get(i).type == RECYCLER_HEAD_ITEM) continue;
            if (mRecyclerModels.get(i).model.get(ShoppingListItemProperties.ID)
                    == id.getId()) {
                mRecyclerModels.removeAt(i);
                break;
            }
        }

        for (Map.Entry<String, Set<Long>> entry : mCategoryMapping.entrySet()) {
            entry.getValue().remove(id.getId());
        }

        mShoppingIds.remove(id.getId());
        mIdToModelMap.remove(id.getId());
        mTagTree.remove(id.getId());
        cleanUpChips();
    }

    private ListMenuButtonDelegate createItemMenuButtonDelegate(
            BookmarkId id, PropertyModel bookmarkModel) {
        return new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                ModelList listItems = new ModelList();
                listItems.add(BasicListMenu.buildMenuListItem(
                        R.string.contextmenu_open_in_new_tab, ItemMenuId.NEW_TAB,
                        R.drawable.new_tab_icon, true));
                listItems.add(BasicListMenu.buildMenuListItem(
                        R.string.contextmenu_open_in_incognito_tab, ItemMenuId.NEW_INCOGNITO_TAB,
                        R.drawable.incognito_simple, true));

                listItems.add(BasicListMenu.buildMenuDivider());

                listItems.add(BasicListMenu.buildMenuListItem(
                        R.string.bookmark_item_edit, ItemMenuId.EDIT,
                        R.drawable.bookmark_edit_active, true));
                listItems.add(BasicListMenu.buildMenuListItem(
                        R.string.bookmark_item_move, ItemMenuId.MOVE_TO,
                        R.drawable.bookmark_move_active, true));
                listItems.add(BasicListMenu.buildMenuListItem(
                        R.string.bookmark_item_delete, ItemMenuId.DELETE,
                        R.drawable.ic_delete_white_24dp, true));

                ListMenu list = new BasicListMenu(mContext, listItems, item -> {
                    if (ItemMenuId.NEW_TAB == item.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        BookmarkBridge.BookmarkItem bookmarkItem =
                                mBookmarkModel.getBookmarkById(id);
                        mRegularTabDelegate.createNewTab(new LoadUrlParams(bookmarkItem.getUrl()),
                                TabLaunchType.FROM_CHROME_UI, null);
                    } else if (ItemMenuId.NEW_INCOGNITO_TAB
                            == item.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        BookmarkBridge.BookmarkItem bookmarkItem =
                                mBookmarkModel.getBookmarkById(id);
                        mIncognitoTabDelegate.createNewTab(new LoadUrlParams(bookmarkItem.getUrl()),
                                TabLaunchType.FROM_CHROME_UI, null);
                    } else if (ItemMenuId.EDIT
                            == item.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        BookmarkUtils.startEditActivity(mContext,
                                BookmarkId.getBookmarkIdFromString(
                                        bookmarkModel.get(ShoppingListItemProperties.ID_STRING)));
                    } else if (ItemMenuId.MOVE_TO
                            == item.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        BookmarkFolderSelectActivity.startFolderSelectActivity(mContext,
                                BookmarkId.getBookmarkIdFromString(
                                        bookmarkModel.get(ShoppingListItemProperties.ID_STRING)));
                        RecordUserAction.record("MobileBookmarkManagerMoveToFolder");
                    } else if (ItemMenuId.DELETE
                            == item.get(ListMenuItemProperties.MENU_ITEM_ID)) {
                        mBookmarkModel.deleteBookmark(
                                BookmarkId.getBookmarkIdFromString(
                                        bookmarkModel.get(ShoppingListItemProperties.ID_STRING)));
                    }
                });
                return list;
            }
        };
    }

    private ViewGroup initializeFilterView(
            SelectableListToolbar.SearchDelegate searchDelegate, int hintStringResId) {
        ViewGroup filterView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.shopping_filter_section, null);

        ViewGroup searchView = filterView.findViewById(R.id.search_view);

        EditText searchEditText = searchView.findViewById(R.id.search_text);
        searchEditText.setHint(hintStringResId);
        //searchEditText.setOnEditorActionListener(this);

        searchView.setBackgroundResource(
                org.chromium.components.browser_ui.widget.R.drawable.search_toolbar_modern_bg);

        ChromeImageButton clearTextButton = searchView.findViewById(R.id.clear_text_button);
        clearTextButton.setOnClickListener(v -> searchEditText.setText(""));
        searchEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                clearTextButton.setVisibility(
                        TextUtils.isEmpty(s) ? View.INVISIBLE : View.VISIBLE);
                searchDelegate.onSearchTextChanged(s.toString());
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void afterTextChanged(Editable s) {}
        });

        mChipList = (ChipList) filterView.findViewById(R.id.chip_list);
        mChipModels = new ModelList();

        final String allChipText = mContext.getResources().getString(R.string.shopping_all_chip);
        mAllCategoryChip = mChipList.createChipListItem(allChipText);
        mAllCategoryChip.model.set(ChipList.ChipProperties.SELECTED, true);
        mChipModels.add(mAllCategoryChip);

        mOtherCategoryChip = mChipList.createChipListItem(
                mContext.getResources().getString(R.string.shopping_other_chip));

        mChipList.init(R.style.ShoppingChip, mChipModels, (selection) -> {
            if (selection != null && selection.equals(mActiveCategory)) return;
            if (selection == null) {
                selection = allChipText;
                mAllCategoryChip.model.set(ChipList.ChipProperties.SELECTED, true);
            }
            mActiveCategory = selection;
            filterShoppingItems(mActiveQuery, mActiveCategory);
        });

        return filterView;
    }

    private void buildChipModels() {
        mChipModels.clear();
        mChipModels.add(mAllCategoryChip);

        ArrayList<Pair<String, Integer>> categoryCounts = new ArrayList<>();
        for (Map.Entry<String, Set<Long>> entry : mCategoryMapping.entrySet()) {
            if (TextUtils.equals(
                    entry.getKey(), mOtherCategoryChip.model.get(ChipList.ChipProperties.TITLE))) {
                continue;
            }
            categoryCounts.add(new Pair<>(entry.getKey(), entry.getValue().size()));
        }

        Pair<String, Integer>[] outArr = new Pair[categoryCounts.size()];
        categoryCounts.toArray(outArr);
        Arrays.sort(outArr, (cat1, cat2) -> cat2.second - cat1.second);

        int indexToScroll = 0;
        for (int i = 0; i < outArr.length; i++) {
            ListItem item = mChipList.createChipListItem(outArr[i].first);
            if (TextUtils.equals(outArr[i].first, mActiveCategory)) {
                item.model.set(ChipList.ChipProperties.SELECTED, true);
                indexToScroll = i;
            }
            mChipModels.add(mChipList.createChipListItem(outArr[i].first));
        }

        Set<Long> otherSet = mCategoryMapping.get(
                mOtherCategoryChip.model.get(ChipList.ChipProperties.TITLE));
        if (otherSet != null && !otherSet.isEmpty()) {
            mChipModels.add(mOtherCategoryChip);
        }
        if (mOtherCategoryChip.model.get(ChipList.ChipProperties.SELECTED)) {
            indexToScroll = mChipModels.size() - 1;
        }
        mChipList.scrollToPosition(indexToScroll);
    }

    private void cleanUpChips() {
        Set<String> removeSet = new HashSet<>();
        for (Map.Entry<String, Set<Long>> entry : mCategoryMapping.entrySet()) {
            if (entry.getValue().isEmpty()) removeSet.add(entry.getKey());
        }

        for (String category : removeSet) {
            mCategoryMapping.remove(category);
        }

        for (int i = 0; i < mChipModels.size(); i++) {
            if (removeSet.contains(mChipModels.get(i).model.get(ChipList.ChipProperties.TITLE))) {
                mChipModels.removeAt(i);
                i--;
            }
        }
        if (mAllShoppingBookmarkModels.isEmpty() && mChipModels.indexOf(mOtherCategoryChip) >= 0) {
            mChipModels.remove(mOtherCategoryChip);
        }
    }

    private void filterShoppingItems(String query, String category) {
        // Start with all bookmarks from the shopping folder and narrow it down.
        List<PropertyModel> filteredModels = mAllShoppingBookmarkModels;
        if (!TextUtils.isEmpty(query)) {
            List<BookmarkId> ids = mBookmarkModel.searchBookmarks(query, 500);
            List<PropertyModel> queryModels = new ArrayList<>();
            for (BookmarkId id : ids) {
                if (!mIdToModelMap.containsKey(id.getId())) continue;
                queryModels.add(mIdToModelMap.get(id.getId()));
            }
            filteredModels = queryModels;

            // Add items that showed up in tag search.
            List<Long> tagSearch = new ArrayList<>();
            String lowercaseQuery = query.toLowerCase(Locale.getDefault());
            mTagTree.search(lowercaseQuery, tagSearch);
            Set<Long> filteredSet = new HashSet<>();
            for (int i = 0; i < filteredModels.size(); i++) {
                filteredSet.add(filteredModels.get(i).get(ShoppingListItemProperties.ID));
            }
            for (int i = 0; i < tagSearch.size(); i++) {
                if (!filteredSet.contains(tagSearch.get(i))) {
                    filteredModels.add(mIdToModelMap.get(tagSearch.get(i)));
                }
            }
        }

        boolean isAllChipSelected = TextUtils.equals(
                category, mAllCategoryChip.model.get(ChipList.ChipProperties.TITLE));

        if (!TextUtils.isEmpty(category) && !isAllChipSelected) {
            List<PropertyModel> categoryModels = new ArrayList<>();
            Set<Long> bookmarksWithCategory = mCategoryMapping.get(category);
            for (int i = 0; i < filteredModels.size(); i++) {
                if (bookmarksWithCategory.contains(
                        filteredModels.get(i).get(ShoppingListItemProperties.ID))) {
                    categoryModels.add(filteredModels.get(i));
                }
            }
            filteredModels = categoryModels;
        }

        if (mRecyclerModels.size() > 1) {
            mRecyclerModels.removeRange(1, mRecyclerModels.size() - 1);
        }
        for (PropertyModel model : filteredModels) {
            mRecyclerModels.add(new ListItem(RECYCLER_SHOPPING_ITEM, model));
        }
    }

    private void makeShoppingRequest(List<BookmarkId> bookmarks) {
        if (bookmarks == null || bookmarks.isEmpty()) return;

        String payload = generateRequestPayload(bookmarks);

        EndpointFetcher.fetchUsingOAuth(this::handleEndpointResponse,
                Profile.getLastUsedRegularProfile(), "SC", ENDPOINT_URL, HTTPS_METHOD, CONTENT_TYPE,
                SCOPES, payload, ENDPOINT_TIMEOUT);
    }

    private void handleEndpointResponse(EndpointResponse response) {
        if (mIsDestroyed) return;

        List<BookmarkId> bookmarks = mBookmarkModel.getChildIDs(mShoppingRoot);
        Map<Long, JSONObject> cache =
                ShoppingCache.updateAndGetCache(response.getResponseString(), bookmarks);

        for (Map.Entry<Long, JSONObject> entry: cache.entrySet()) {
            try {
                long id = entry.getValue().getLong("id");
                updateItemWithMeta(id, cache.get(id));
            } catch (JSONException ex) {
                // noop
            }
        }
    }

    private void updateItemWithMeta(long id, JSONObject item) {
        if (item == null) return;

        if (mImageFetcher == null) {
            mImageFetcher = ImageFetcherFactory.createImageFetcher(
                    ImageFetcherConfig.NETWORK_ONLY, Profile.getLastUsedRegularProfile());
        }

        try {
            JSONObject cardData = ShoppingCache.safeGetJSONObject(item, "card");

            // Card data is required, if null, skip this item.
            if (cardData == null) return;

            String imageUrl = ShoppingCache.safeGetJSONString(cardData, "imageUrl");
            //String url = curItem.has("url") ? curItem.getString("url") : null;
            //String domain = url != null ? new GURL(url).getHost() : null;

            long bookmarkId = item.has("id") ? item.getLong("id") : -1;

            JSONArray categories =
                    item.has("categories") ? item.getJSONArray("categories") : null;
            if (categories == null || categories.length() == 0) {
                String otherString =
                        mOtherCategoryChip.model.get(ChipList.ChipProperties.TITLE);
                if (!mCategoryMapping.containsKey(otherString)) {
                    mCategoryMapping.put(otherString, new HashSet<>());
                }
                mCategoryMapping.get(otherString).add(bookmarkId);
            } else {


                String otherString =
                        mOtherCategoryChip.model.get(ChipList.ChipProperties.TITLE);
                Set<Long> otherSet = mCategoryMapping.get(otherString);
                if (otherSet != null) otherSet.remove(bookmarkId);

                for (int j = 0; j < categories.length(); j++) {
                    JSONObject curCategory = categories.getJSONObject(j);
                    if (curCategory.has("displayableValue")) {
                        String categoryString = curCategory.getString("displayableValue");
                        if (!mCategoryMapping.containsKey(categoryString)) {
                            mCategoryMapping.put(categoryString, new HashSet<>());
                        }
                        mCategoryMapping.get(categoryString).add(bookmarkId);
                    }
                }
            }

            JSONArray tags = item.has("tags") ? item.getJSONArray("tags") : null;
            if (tags != null) {
                for (int j = 0; j < tags.length(); j++) {
                    JSONObject curTag = tags.getJSONObject(j);
                    if (curTag.has("displayableValue")) {
                        mTagTree.add(
                                curTag.getString("displayableValue").toLowerCase(
                                        Locale.getDefault()), bookmarkId);
                    }
                }
            }

            PropertyModel model = mIdToModelMap.get(bookmarkId);

            // The bookmark may have been moved out of the shopping folder, if the model isn't
            // in our set, ignore it.
            if (model == null) return;

            model.set(ShoppingListItemProperties.TITLE, cardData.getString("title"));

            CurrencyFormatter formatter = null;

            JSONObject dataObject = ShoppingCache.safeGetJSONObject(cardData, "data");
            JSONObject priceDataObject = ShoppingCache.safeGetJSONObject(dataObject, "priceData");
            JSONObject curPriceObject =
                    ShoppingCache.safeGetJSONObject(priceDataObject, "currentPrice");
            if (curPriceObject != null) {
                String micros = curPriceObject.has("amountInMicros")
                        ? curPriceObject.getString("amountInMicros")
                        : null;
                String code = curPriceObject.has("currencyCode")
                        ? curPriceObject.getString("currencyCode")
                        : null;
                if (code != null) {
                    formatter = new CurrencyFormatter(code, Locale.getDefault());
                    formatter.setMaximumFractionalDigits(0);
                }
                if (micros != null && formatter != null) {
                    try {
                        long microsParsed = Long.parseLong(micros);
                        double currencyValue = microsParsed / 1000000d;
                        model.set(ShoppingListItemProperties.PRICE,
                                formatter.format("" + currencyValue));
                    } catch (NumberFormatException ex) {
                        model.set(ShoppingListItemProperties.PRICE, null);
                    }
                }
            }

            JSONObject priceRangeObject =
                    ShoppingCache.safeGetJSONObject(priceDataObject, "priceRange");
            if (priceRangeObject != null) {
                String highestMicros = priceRangeObject.has("highestAmountInMicros")
                        ? priceRangeObject.getString("highestAmountInMicros")
                        : null;
                String lowestMicros = priceRangeObject.has("lowestAmountInMicros")
                        ? priceRangeObject.getString("lowestAmountInMicros")
                        : null;
                try {
                    if (formatter != null && highestMicros != null && lowestMicros != null) {
                        long highestParsed = Long.parseLong(highestMicros);
                        double highestValue = highestParsed / 1000000d;

                        long lowestParsed = Long.parseLong(lowestMicros);
                        double lowestValue = lowestParsed / 1000000d;

                        String rangeString = formatter.format("" + lowestValue) + " - "
                                + formatter.format("" + highestValue);

                        // Immediately dump a notification if a price range is available.
                        BookmarkId idObj = BookmarkId.getBookmarkIdFromString(
                                model.get(ShoppingListItemProperties.ID_STRING));

                        String title = mContext.getResources().getString(
                                R.string.shopping_price_range_title,
                                model.get(ShoppingListItemProperties.TITLE), rangeString);

                        String subtitle;
                        boolean notificationOpensSite = false;
                        if (notificationOpensSite) {
                            subtitle = mContext.getResources().getString(
                                    R.string.shopping_price_range_subtitle,
                                    model.get(ShoppingListItemProperties.DOMAIN));
                        } else {
                            subtitle = mContext.getResources().getString(
                                    R.string.shopping_notification_view_in_folder);
                        }

                        if (!ShoppingNotificationService.hasPendingNotification()) {
                            JSONObject notificationInfo = new JSONObject();
                            notificationInfo.put("id", 0);
                            notificationInfo.put("title", title);
                            notificationInfo.put("subtitle", subtitle);
                            notificationInfo.put("url",
                                    mBookmarkModel.getBookmarkById(idObj).getUrl().getSpec());
                            notificationInfo.put("bookmark_id", idObj.toString());
                            notificationInfo.put("creationTime", System.currentTimeMillis());
                            SharedPreferencesManager.getInstance().writeString(
                                    ChromePreferenceKeys.SHOPPING_CURRENT_NOTIFICATION,
                                    notificationInfo.toString());

                            BackgroundTaskScheduler scheduler =
                                    BackgroundTaskSchedulerFactory.getScheduler();
                            long triggerTime = System.currentTimeMillis()
                                    + ShoppingNotificationService.NOTIFICATION_INTERVAL;
                            TaskInfo taskInfo; /*= TaskInfo.createTask(
                                    TaskIds.SHOPPING_SEND_NOTIFICATION_ID,
                                    TaskInfo.ExactInfo.create().setTriggerAtMs(
                                            triggerTime).build()).build();*/
                            TaskInfo.OneOffInfo info =
                                    TaskInfo.OneOffInfo.create()
                                            .setWindowStartTimeMs(
                                                    ShoppingNotificationService.NOTIFICATION_INTERVAL)
                                            .setWindowEndTimeMs(
                                                    (int) (ShoppingNotificationService.NOTIFICATION_INTERVAL * 1.5f))
                                            .build();
                            taskInfo = TaskInfo.createTask(
                                    TaskIds.SHOPPING_SEND_NOTIFICATION_ID, info).build();

                            scheduler.schedule(mContext, taskInfo);
                        }
                    }

                } catch (NumberFormatException ex) {
                    // noop
                }
            }

            if (imageUrl != null) {
                if (mProductImageCache.getBitmap(imageUrl) != null) {
                    model.set(ShoppingListItemProperties.IMAGE,
                            mProductImageCache.getBitmap(imageUrl));
                } else {
                    mImageFetcher.fetchImage(ImageFetcher.Params.create(imageUrl, "SC"),
                            (bitmap) -> {
                                if (mIsDestroyed) return;
                                if (bitmap == null) {
                                    updateModelWithFavicon(model);
                                } else {
                                    mProductImageCache.putBitmap(imageUrl, bitmap);
                                    model.set(ShoppingListItemProperties.USING_FAVICON, false);
                                    model.set(ShoppingListItemProperties.IMAGE, bitmap);
                                }
                            });
                }
            } else {
                updateModelWithFavicon(model);
            }

            buildChipModels();

        } catch (JSONException e) {
            // Couldn't parse the response.
        }
    }

    private void updateModelWithFavicon(PropertyModel model) {
        if (mIsDestroyed) return;
        LargeIconBridge iconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        BookmarkId idObj = BookmarkId.getBookmarkIdFromString(
                model.get(ShoppingListItemProperties.ID_STRING));
        GURL gurl = mBookmarkModel.getBookmarkById(idObj).getUrl();
        int desiredSize = mContext.getResources().getDimensionPixelSize(
                R.dimen.shopping_notification_icon_size);

        LargeIconBridge.LargeIconCallback callback = new LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(Bitmap icon, int fallbackColor,
                    boolean isFallbackColorDefault, int iconType) {
                if (mIsDestroyed) return;
                if (icon != null) {
                    model.set(ShoppingListItemProperties.USING_FAVICON, true);
                    model.set(ShoppingListItemProperties.IMAGE, icon);
                    return;
                }

                FaviconHelper smallIconHelper = new FaviconHelper();
                smallIconHelper.getForeignFaviconImageForURL(
                        Profile.getLastUsedRegularProfile(), gurl.getSpec(),
                        desiredSize,
                        new FaviconHelper.FaviconImageCallback() {
                            @Override
                            public void onFaviconAvailable(Bitmap image, String iconUrl) {
                                if (mIsDestroyed) return;
                                model.set(ShoppingListItemProperties.USING_FAVICON, true);
                                model.set(ShoppingListItemProperties.IMAGE, image);
                            }
                        });
            }
        };
        iconBridge.getLargeIconForUrl(gurl, desiredSize, callback);
    }

    private String generateRequestPayload(List<BookmarkId> bookmarks) {
        JSONObject root = new JSONObject();

        JSONArray bookmarkObjects = new JSONArray();

        for (BookmarkId id : bookmarks) {
            BookmarkBridge.BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            JSONObject curObj = new JSONObject();
            try {
                curObj.put("url", item.getUrl().getSpec());
                curObj.put("title", item.getTitle());
                curObj.put("id", id.getId());
                bookmarkObjects.put(curObj);
            } catch (JSONException e) {
                // Do nothing, skip this item.
            }
        }

        try {
            root.put("bookmarks", bookmarkObjects);
        } catch (JSONException e) {
            // Do nothing, return an object with an empty root.
        }

        return root.toString();
    }

    public void destroy() {
        mIsDestroyed = true;
    }
}
