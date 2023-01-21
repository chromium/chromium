package com.ark.browser.ui.fragment.collection;

import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.viewpager.widget.ViewPager;

import com.ark.browser.ui.adapter.PageAdapter;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.ui.fragment.collection.bookmark.BookmarkFragment;
import com.ark.browser.ui.fragment.collection.history.HistoryFragment;
import com.ark.browser.ui.fragment.collection.offline.OfflinePageFragment;
import com.ark.browser.ui.fragment.collection.password.SavedPasswordsFragment;
import com.ark.browser.ui.fragment.dialog.RecyclerPartShadowDialogFragment;
import com.ark.browser.ui.widget.CustomHeaderLayout;
import com.zpj.toast.ZToast;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.List;

public class CollectionFragment extends BaseSwipeBackFragment implements CustomHeaderLayout.Callback {

    private static final String TAG = "CollectionFragment";
    private static final String KEY_POSITION = "key_position";
    private static final String[] TAB_TITLES = {"书签", "历史", "网页", "密码"};

    private final List<CollectionChildFragment> fragments = new ArrayList<>();

    private CollectionChildFragment bookmarkFragment;

    private CustomHeaderLayout mHeaderLayout;
    private ViewPager mViewPager;

//    public static void start() {
//        ZBus.post(new CollectionFragment2());
//    }

//    public static void start(Context context) {
//        start(context, 0);
//    }
//
//    public static void start(Context context, int position) {
//        newInstance(position).show(context);
//    }

    public static CollectionFragment newInstance() {
        return newInstance(0);
    }

    public static CollectionFragment newInstance(int position) {

        Bundle args = new Bundle();
        args.putInt(KEY_POSITION, position);
        CollectionFragment fragment = new CollectionFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_collections;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        int position = 0;
        if (getArguments() != null) {
            position = getArguments().getInt(KEY_POSITION, 0);
        }

        bookmarkFragment = findChildFragment(BookmarkFragment.class);
        if (bookmarkFragment == null) {
            bookmarkFragment = new BookmarkFragment();
            HistoryFragment historyFragment = new HistoryFragment();
            OfflinePageFragment offlinePageFragment = new OfflinePageFragment();
            SavedPasswordsFragment savedPasswordsFragment = new SavedPasswordsFragment();
            fragments.add(bookmarkFragment);
            fragments.add(historyFragment);
            fragments.add(offlinePageFragment);
            fragments.add(savedPasswordsFragment);
        } else {
            fragments.add(bookmarkFragment);
            fragments.add(findChildFragment(HistoryFragment.class));
            fragments.add(findChildFragment(OfflinePageFragment.class));
            fragments.add(findChildFragment(SavedPasswordsFragment.class));
        }

        mHeaderLayout = findViewById(R.id.header_layout);
        mHeaderLayout.setCallback(this);
        mViewPager = mHeaderLayout.getViewPager();

        FrameLayout flBottomBar = findViewById(R.id.fl_bottom_bar);
        bookmarkFragment.onCreateBottomBar(context, flBottomBar);
        PageAdapter pageAdapter = new PageAdapter(getChildFragmentManager(), fragments, TAB_TITLES);
        mViewPager.setAdapter(pageAdapter);
        mViewPager.setOffscreenPageLimit(fragments.size());
        mViewPager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageScrolled(int i, float v, int i1) {

            }

            @Override
            public void onPageSelected(int i) {
                ZToast.error("i=" + i);
//                    flBottomBar.removeAllViews();
                fragments.get(i).onCreateBottomBar(context, flBottomBar);
            }

            @Override
            public void onPageScrollStateChanged(int i) {

            }
        });
        mViewPager.setCurrentItem(position);
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mHeaderLayout != null) {
            mHeaderLayout.onResume();
        }
    }

    @Override
    public boolean onBackPressedSupport() {
        if (mHeaderLayout.isSearchMode()) {
            mHeaderLayout.exitSearch();
            return true;
        }
        return super.onBackPressedSupport();
    }

    @Override
    public void onBackButtonClicked(View view) {
        if (mHeaderLayout.isSearchMode()) {
            mHeaderLayout.exitSearch();
            return;
        }
        pop();
    }

    @Override
    public void onMenuButtonClicked(View view) {
        fragments.get(mViewPager.getCurrentItem()).onShowMenu(view);
    }

    @Override
    public void onTitleClicked(View view, String[] titles, int position, Runnable dismissRunnable) {
        new RecyclerPartShadowDialogFragment()
                .addItems(titles)
                .setSelectedItem(position)
                .setOnItemClickListener(new RecyclerPartShadowDialogFragment.OnItemClickListener() {
                    @Override
                    public void onItemClick(View view, String title, int position) {
                        mViewPager.setCurrentItem(position, true);
                    }
                })
                .setOnDismissListener(dialog -> {
                    if (dismissRunnable != null) {
                        dismissRunnable.run();
                    }
                })
                .show(view);
    }
}

