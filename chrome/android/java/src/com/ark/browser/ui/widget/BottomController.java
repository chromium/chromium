package com.ark.browser.ui.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.ark.browser.ui.fragment.dialog.MainMenuDialog;
import com.ark.browser.ui.widget.indicator.CoolIndicator;
import com.ark.browser.utils.ArkLogger;
import com.zpj.utils.KeyboardUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

public class BottomController {

    private static final String TAG = "BottomController";

    private final Context mContext;

    private final CoolIndicator mProgressBar;

//    private final FrameLayout mCustomToolbar;
//    private final LinearLayout mBottomLoadedBar;

    private final ImageView menuButton;
    private final ImageView toolButton;
    private final ImageView starButton;
    private final ImageView loadingCancel;

    private final TextView loadingTitle;

    private final EmptyTabObserver mTabObserver;

    private Tab mPage;
    private boolean mIsIncognito;
    private int mPrimaryColor;


    public BottomController(View view) {
        mContext = view.getContext();
//        mCustomToolbar = view.findViewById(R.id.custom_bottom_bar);
//        mBottomLoadedBar = view.findViewById(R.id.bottom_loaded_view);

        mProgressBar = view.findViewById(R.id.cool_progress_bar);
        mProgressBar.setMax(100);

        menuButton = view.findViewById(R.id.btn_menu);
        menuButton.setOnClickListener(v -> new MainMenuDialog().show(mContext));
        menuButton.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
//                TabActionDialog.newInstance().show(v);
                return true;
            }
        });

        toolButton = view.findViewById(R.id.btn_tools);
        toolButton.setOnClickListener(v -> {
//            if (mCurrentPageInfo != null) {
//                ToolsDialog.newInstance(mCurrentPageInfo.getPageId()).show(context);
//            }
        });

        starButton = view.findViewById(R.id.btn_star);
        starButton.setOnClickListener((View.OnClickListener) v -> {
//            CollectionEditorDialog.newInstance((BookmarkId) null).show(v.getContext());
        });

        loadingTitle = view.findViewById(R.id.loading_title);

        loadingCancel = view.findViewById(R.id.loading_cancel);
        loadingCancel.setOnClickListener(v -> {
//            Tab tab = PageCacheManager.getInstance().findPage(mCurrentPageInfo);
//            if (tab != null) {
//                if (tab.isLoading()) {
//                    loadingCancel.setImageResource(R.drawable.qianxun_refresh);
//                    tab.stopLoading();
//                } else {
//                    loadingCancel.setImageResource(R.drawable.qianxun_cancel);
//                    tab.reload();
//                }
//            }
        });


        mTabObserver = new EmptyTabObserver() {

            @Override
            public void onAttachToWindowAndroid(Tab tab, @NonNull WindowAndroid windowAndroid) {
                ArkLogger.e(TAG, "onAttachToWindowAndroid");
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
                if (toDifferentDocument) {
                    if (tab.getProgress() > 5 && tab.getProgress() < 100) {
                        updateLoadProgress(100);
                    }
                }
                finishLoadProgress(true);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                updateLoadProgress((int) (progress * 100));
            }

            @Override
            public void onContentChanged(Tab tab) {
                Log.d(TAG, "onContentChanged");
                loadingTitle.setText(tab.getTitle());
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                Log.d(TAG, "onWebContentsSwapped didStartLoad=" + didStartLoad + "  didFinishLoad=" + didFinishLoad);
                if (!didStartLoad) return;
            }

            @Override
            public void onUpdateUrl(Tab tab, GURL url) {
                loadingTitle.setText(url.getSpec());
            }
        };
    }

    /**
     * 更新收藏按钮状态
     *
     * @param tab 当前tab
     */
    public void updateStarButton(Tab tab) {
//        if (mModel == null) {
//            mModel = new BookmarkModel();
//            mModel.addObserver(mBookmarksObserver);
//        }
//        if (tab != null
//                && (mModel.doesBookmarkExist(new BookmarkId(tab.getBookmarkId(), BookmarkType.NORMAL))
//                || HomepageManager.getFavoriteByUrl(tab.getUrl()) != null)) { // writer.hasItemInDatabase(tab.getUrl())
//            starButton.setImageResource(R.drawable.ic_bookmark_added);
//        } else {
//            starButton.setImageResource(R.drawable.ic_add_bookmark);
//        }
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
        page.addObserver(mTabObserver);
        mPage = page;
        mIsIncognito = page.isIncognito();
        loadingTitle.setText(page.getTitle());
        int drawableId = page.isLoading() ? R.drawable.ic_cancel : R.drawable.ic_refresh;
        loadingCancel.setImageResource(drawableId);
        if (page.isLoading()) {
            mProgressBar.start();
        }

        updateStarButton(page);
    }

    public void onPageDetached(@NonNull Tab page) {
        ArkLogger.e(TAG, "onPageDetached page=" + page.getId());
        page.removeObserver(mTabObserver);
        finishLoadProgress(false);
    }


}

