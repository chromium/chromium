package com.ark.browser.ui.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.android.launcher3.database.HomepageManager;
import com.ark.browser.core.bookmark.BookmarkBridge;
import com.ark.browser.core.bookmark.BookmarkModel;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.ui.fragment.dialog.CollectionEditorDialog;
import com.ark.browser.ui.fragment.dialog.MainMenuDialog;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.ark.browser.ui.fragment.dialog.ToolsDialog;
import com.ark.browser.ui.fragment.search.SearchFragment;
import com.ark.browser.ui.widget.indicator.CoolIndicator;
import com.ark.browser.utils.ArkLogger;
import com.zpj.utils.KeyboardUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

public class BottomController {

    private static final String TAG = "BottomController";

    private final Context mContext;

    private final CoolIndicator mProgressBar;

//    private final FrameLayout mCustomToolbar;
//    private final LinearLayout mBottomLoadedBar;

    private final View mTitleBar;
    private final ImageView menuButton;
    private final ImageView toolButton;
    private final ImageView starButton;
    private final ImageView loadingCancel;

    private final TextView loadingTitle;

    private final EmptyTabObserver mTabObserver;

    private Tab mTab;
    private boolean mIsIncognito;
    private int mPrimaryColor;

    private BookmarkModel mBookmarkModel;

    private final BookmarkBridge.BookmarkModelObserver mBookmarksObserver = new BookmarkBridge.BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            updateStarButton(mTab);
        }
    };

    private final int mDarkModeTint;
    private final int mLightModeTint;


    public BottomController(View view) {
        mContext = view.getContext();
//        mCustomToolbar = view.findViewById(R.id.custom_bottom_bar);
//        mBottomLoadedBar = view.findViewById(R.id.bottom_loaded_view);

        mDarkModeTint = Color.parseColor("#A6000000");
        mLightModeTint = Color.WHITE;

        mTitleBar = view.findViewById(R.id.title_bar);

        mProgressBar = view.findViewById(R.id.cool_progress_bar);
        mProgressBar.setMax(100);

        menuButton = view.findViewById(R.id.btn_menu);
        menuButton.setOnClickListener(v -> new MainMenuDialog().show(mContext));
        menuButton.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                TabActionDialog.newInstance(mTab.getId()).show(v);
                return true;
            }
        });

        toolButton = view.findViewById(R.id.btn_tools);
        toolButton.setOnClickListener(v -> {
            ToolsDialog.start(mContext, (ArkTabImpl) mTab);
        });

        starButton = view.findViewById(R.id.btn_star);
        starButton.setOnClickListener((View.OnClickListener) v -> {
//            CollectionEditorDialog.newInstance((BookmarkId) null).show(v.getContext());
            if (mTab == null) {
                return;
            }

            CollectionEditorDialog.newInstance(mTab).show(v.getContext());

//            BookmarkId bookmarkId = mBookmarkModel.getUserBookmarkIdForTab(mPage);
//            if (bookmarkId == null) {
//                mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(),
//                        0, mPage.getTitle(), mPage.getUrl());
//            } else {
//                mBookmarkModel.deleteBookmark(bookmarkId);
//            }

        });

        loadingTitle = view.findViewById(R.id.loading_title);
        loadingTitle.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                new SearchFragment().show(mContext);
            }
        });

        loadingCancel = view.findViewById(R.id.loading_cancel);
        loadingCancel.setOnClickListener(v -> {
            if (mTab == null) {
                return;
            }
            if (mTab.isLoading()) {
                loadingCancel.setImageResource(R.drawable.ic_refresh);
                mTab.stopLoading();
            } else {
                loadingCancel.setImageResource(R.drawable.ic_cancel);
                mTab.reload();
            }
        });


        mTabObserver = new EmptyTabObserver() {

            @Override
            public void onAttachToWindowAndroid(Tab tab, @NonNull WindowAndroid windowAndroid) {
                ArkLogger.e(TAG, "onAttachToWindowAndroid");
                updateStarButton(tab);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                loadingTitle.setText(tab.getTitle());
            }

            @Override
            public void onShown(Tab tab, int type) {
            }

            @Override
            public void onCrash(Tab tab) {
                finishLoadProgress(false);
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {

            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                ArkLogger.d(TAG, "onPageLoadStarted tab=" + tab.getId());
                updateStarButton(tab);
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                ArkLogger.d(TAG, "onLoadStarted tab=" + tab.getId());
                KeyboardUtils.hideSoftInputKeyboard(
                        tab.getWindowAndroid().getActivity().get().getWindow().getDecorView());
//                mProgressBar.setVisibility(View.VISIBLE);
                mProgressBar.start();
                loadingCancel.setImageResource(R.drawable.ic_cancel);
                loadingTitle.setText(tab.getUrl().toString());
                updateStarButton(tab);
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                super.onFaviconUpdated(tab, icon);
//                faviconImg.setImageDrawable(new BitmapDrawable(faviconImg.getResources(), icon));
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
//                progressBar.setVisibility(View.GONE);
                if (TextUtils.isEmpty(tab.getTitle())) {
                    loadingTitle.setText(tab.getUrl().toString());
                } else {
                    loadingTitle.setText(tab.getTitle());
                }
                loadingCancel.setImageResource(R.drawable.ic_refresh);
//                faviconImg.setImageDrawable(new BitmapDrawable(faviconImg.getResources(), tab.getFavicon()));
//                if (toDifferentDocument) {
//                    if (tab.getProgress() > 5 && tab.getProgress() < 100) {
//                        updateLoadProgress(100);
//                    }
//                    updateLoadProgress(100);
//                }
                finishLoadProgress(true);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                updateLoadProgress((int) (progress * 100));
            }

            @Override
            public void onContentChanged(Tab tab) {
                Log.e(TAG, "onContentChanged");
                loadingTitle.setText(tab.getTitle());
                updateStarButton(tab);
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                Log.e(TAG, "onWebContentsSwapped didStartLoad=" + didStartLoad + "  didFinishLoad=" + didFinishLoad);
                if (!didStartLoad) return;
                if (didFinishLoad) {
                    finishLoadProgress(true);
                } else if (!mProgressBar.isRunning()) {
                    updateLoadProgress((int) (tab.getProgress() * 100));
                }
            }

            @Override
            public void onUpdateUrl(Tab tab, GURL url) {
                loadingTitle.setText(url.getSpec());
                updateStarButton(tab);
            }

        };
    }

    public void updateStarButton() {
        updateStarButton(mTab);
    }

    /**
     * 更新收藏按钮状态
     *
     * @param tab 当前tab
     */
    public void updateStarButton(Tab tab) {
        if (mBookmarkModel == null) {
            mBookmarkModel = new BookmarkModel();
            mBookmarkModel.addObserver(mBookmarksObserver);
            mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
        }
        if (!mBookmarkModel.isBookmarkModelLoaded()) {
            mBookmarkModel.finishLoadingBookmarkModel(() -> updateStarButton(tab));
            return;
        }
        if (tab != null && (mBookmarkModel.hasBookmarkIdForTab(tab)
                || HomepageManager.getFavoriteByUrl(tab.getUrl().getSpec()) != null)) {
            starButton.setImageResource(R.drawable.ic_bookmark_added);
        } else {
            starButton.setImageResource(R.drawable.ic_add_bookmark);
        }
    }

    private void updateLoadProgress(int progress) {
        ArkLogger.d(TAG, "updateLoadProgress progress=" + progress);
        mProgressBar.start();
//        if (progress >= 100) {
//            mProgressBar.setVisibility(View.GONE);
//        } else {
//            mProgressBar.setVisibility(View.VISIBLE);
//            mProgressBar.setProgress((int) (progress * 100));
//        }
    }

    private void finishLoadProgress(boolean delayed) {
        ArkLogger.e(TAG, "finishLoadProgress");
//        mProgressBar.setVisibility(View.GONE);
        mProgressBar.complete();
    }


    public void onPageAttached(@NonNull Tab page) {
        ArkLogger.e(TAG, "onPageAttached page=" + page.getId());
        if (mTab != page) {
            if (mTab != null) {
                mTab.removeObserver(mTabObserver);
            }
            page.addObserver(mTabObserver);
            mTab = page;
        }
        mIsIncognito = page.isIncognito();
        loadingTitle.setText(page.getTitle());
        int drawableId = page.isLoading() ? R.drawable.ic_cancel : R.drawable.ic_refresh;
        loadingCancel.setImageResource(drawableId);
        if (page.isLoading()) {
            mProgressBar.start();
        } else {
            mProgressBar.complete();
        }

        updateStarButton(page);
    }

    public void onPageDetached(@NonNull Tab page) {
        ArkLogger.e(TAG, "onPageDetached page=" + page.getId());
        page.removeObserver(mTabObserver);
        if (mTab == page) {
            finishLoadProgress(false);
            mTab = null;
        }
    }

    public void onDestroy() {
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
            mTab = null;
        }
        if (mBookmarkModel != null) {
            mBookmarkModel.removeObserver(mBookmarksObserver);
            mBookmarkModel.destroy();
            mBookmarkModel = null;
        }
    }

    public void updatePrimaryColor(int color) {
        if (AppConfig.isNightMode()) {
            color = Color.BLACK;
        }

        boolean colorChanged = mPrimaryColor != color;
        Log.d("updatePrimaryColor", "colorChanged:" + colorChanged);
        Log.d("updatePrimaryColor", "color=" + color);
//        if (!colorChanged) return;

        mPrimaryColor = color;
        int bgColor;
        if (color == Color.WHITE) {
            bgColor = Color.parseColor("#f6f6f6");
        } else if (color == Color.BLACK) {
            bgColor = Color.parseColor("#191F2D");
        } else {
            bgColor = ColorUtils.getDarkenedColor(color, 0.97f);
        }

        boolean useLight = ColorUtils.shouldUseLightForegroundOnBackground(color);
//        faviconImg.setBorderColor(useLight ? Color.WHITE : mContext.getResources().getColor(R.color.google_black_400));
        GradientDrawable gradientDrawable = new GradientDrawable();
        gradientDrawable.setCornerRadius(ScreenUtils.dp2px(10));
        gradientDrawable.setColor(bgColor);
        mTitleBar.setBackground(gradientDrawable);
        int tint = useLight ? mLightModeTint : mDarkModeTint;
        menuButton.setColorFilter(tint);
        toolButton.setColorFilter(tint);
        starButton.setColorFilter(tint);
        loadingCancel.setColorFilter(tint);
        int textColor = useLight ? Color.WHITE : mContext.getResources().getColor(R.color.google_black_400);
        loadingTitle.setTextColor(textColor);
    }


}

