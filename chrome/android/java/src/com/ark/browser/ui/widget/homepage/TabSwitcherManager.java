package com.ark.browser.ui.widget.homepage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.animation.DecelerateInterpolator;

import androidx.annotation.NonNull;

import com.android.launcher3.BubbleTextView;
import com.android.launcher3.ItemInfoWithIcon;
import com.android.launcher3.LauncherLayout;
import com.android.launcher3.TabItemInfo;
import com.ark.browser.core.ArkCompositorViewHolder;
import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.core.ArkWindowAndroid;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.TabManagerObserver;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.collection.CollectionFragment;
import com.ark.browser.ui.fragment.dialog.MainMenuDialog;
import com.ark.browser.ui.fragment.download.DownloadFragment2;
import com.ark.browser.ui.fragment.manager.ManagerFragment;
import com.ark.browser.ui.fragment.search.SearchFragment;
import com.ark.browser.ui.fragment.settings.SettingsFragment;
import com.ark.browser.ui.widget.BottomControlBar;
import com.ark.browser.ui.widget.BottomController;
import com.ark.browser.utils.ArkLogger;
import com.zpj.utils.ContextUtils;
import com.zpj.utils.StatusBarUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.Toast;

public class TabSwitcherManager implements SwitcherRecyclerLayout.Callback {


    private final Context mContext;
    private final ArkCompositorViewHolder mViewHolder;
    private final View mBrowserLayout;
    private final ArkLauncherLayout mLauncherLayout;
    private final TabSwitcherLayout mTabSwitcherLayout;
    private final SwitcherRecyclerLayout mSwitcher;
    private final BottomController mBottomController;

    public TabSwitcherManager(View view, Bundle savedInstanceState) {
        mContext = view.getContext();
        mViewHolder = view.findViewById(R.id.compositor_view_holder);
        mViewHolder.setRootView(view);
        mLauncherLayout = view.findViewById(R.id.launcher_layout);
        mBrowserLayout = view.findViewById(R.id.browser_layout);
        mTabSwitcherLayout = view.findViewById(R.id.tab_switcher_layout);
        mSwitcher = mTabSwitcherLayout.getSwitcher();
        mSwitcher.addCallback(this);
        mSwitcher.setAdapter(new ArkTabAdapter(mContext, mViewHolder.getTabContentManager()));

        BottomControlBar bottomControlBar = view.findViewById(R.id.bottom_control_bar);
        bottomControlBar.setSwitcherManager(this);
        mBottomController = new BottomController(view);

        mLauncherLayout.init(savedInstanceState);
        mLauncherLayout.setClickHandler(new LauncherLayout.ClickHandler() {
            @Override
            public void onClickAppShortcut(View v, ItemInfoWithIcon itemInfo) {
                Toast.makeText(mContext, "title=" + itemInfo.title + " url=" + itemInfo.url, Toast.LENGTH_SHORT).show();
                if (HomepageUtils.isDeepLink(itemInfo.url)) {
                    switch (itemInfo.url) {
                        case HomepageUtils.DEEPLINK_MANAGER:
                            new ManagerFragment().show(mContext);
                            break;
                        case HomepageUtils.DEEPLINK_COLLECTIONS:
                            CollectionFragment.newInstance(0).show(mContext);
                            break;
                        case HomepageUtils.DEEPLINK_BROWSER:
                            new MainMenuDialog().show(mContext);
                            break;
                        case HomepageUtils.DEEPLINK_DOWNLOADS:
                            new DownloadFragment2().show(mContext);
                            break;
                        case HomepageUtils.DEEPLINK_SETTINGS:
                            new SettingsFragment().show(mContext);
                            break;
                    }
                } else {
                    LoadUrlParams params = new LoadUrlParams(itemInfo.url);
                    getCurrentTabGroup().openInNewTab(params, TabLaunchType.FROM_LAUNCHER_SHORTCUT);
                    if (v instanceof BubbleTextView) {
                        Rect rect = new Rect();
                        ((BubbleTextView) v).getIconBounds(rect);

                        int[] location = new int[2];
                        v.getLocationOnScreen(location);

                        int x = location[0] + rect.centerX();
                        int y = location[1] + rect.centerY();

                        NewTabTransformAnimation.with(mContext)
                                .setRect(rect)
                                .setCenterPosition(x, y)
                                .onAnimationStart(() -> {
                                    Toast.makeText(mContext, "start animation", Toast.LENGTH_SHORT).show();
                                })
                                .onAnimationEnd(() -> {
                                    Toast.makeText(mContext, "dismiss animation", Toast.LENGTH_SHORT).show();
                                    goToBrowser();
                                })
                                .start();

                    } else {
                        goToBrowser();
                    }
                }
            }

            @Override
            public void onClickTabCard(View v, TabItemInfo itemInfo) {
                Toast.makeText(mContext, "title=" + itemInfo.title + " url=" + itemInfo.url, Toast.LENGTH_SHORT).show();
            }
        });

        mLauncherLayout.setSlideListener(new LauncherLayout.SlideListener() {
            @Override
            public void onSlideStart(int direction) {
//                if (!isInTabSwitcher()) {
//                    goToTabSwitcher();
//                }
                if (direction == 1) {
                    new SearchFragment().show(mLauncherLayout.getContext());
                } else if (isInLauncher()) {
                    mSwitcher.open();
                }
            }

            @Override
            public void onSlideVertical(float dy, int direction) {

            }

            @Override
            public void onSlideEnd() {

            }

            @Override
            public boolean canHandleLongPress() {
                return isInLauncher();
            }

            @Override
            public boolean canStartDrag() {
                return isInLauncher();
            }
        });

    }

    public void initCompositor(ArkWindowAndroid window) {
        TabGroupManager.global().addObserver(new TabManagerObserver() {
            @Override
            public void onChange() {
                ITab tab = TabGroupManager.global().getCurrentTab();
                ArkLogger.d(this, "TabManagerObserver onChange tab=" + tab);
                if (tab != null) {
                    mViewHolder.getLayoutManager().initLayoutTabFromHost(tab.getId());
                }
                mViewHolder.setTab(TabCacheManager.getInstance().findTab(tab));
            }

            @Override
            public void onGroupChanged(ITabGroup newGroup, ITabGroup oldGroup) {
                ArkLogger.e(this, "TabManagerObserver onGroupChanged newGroup="
                        + newGroup + " oldGroup=" + oldGroup);
                mSwitcher.setTabGroup(newGroup);
            }

            @Override
            public void onTabSelected(ITab tab) {
                ArkLogger.d(this, "TabManagerObserver onTabSelected tab=" + tab);
                mSwitcher.setTabGroup(tab.getParentGroup());
                mViewHolder.getLayoutManager().initLayoutTabFromHost(tab.getId());
                mViewHolder.setTab(TabCacheManager.getInstance().findTab(tab));
                showBrowser();
            }

            @Override
            public void onTabMoved(ITab tab, ITabGroup oldGroup) {
                Tab nativeTab = mViewHolder.getTab();
                int id = nativeTab == null ? -1 : nativeTab.getId();
                ArkLogger.e(this, "TabManagerObserver onTabMoved id="
                        + tab.getId() + " currentId=" + id);
                if (id == tab.getId()) {
                    onTabSelected(tab);
                } else if (oldGroup == mSwitcher.getTabGroup()
                        || tab.getParentGroup() == mSwitcher.getTabGroup()) {
                    mSwitcher.notifyDataSetChanged();
                }
            }
        });

        mViewHolder.setFocusable(false);
        mViewHolder.initCompositor(window, new ArkCompositorViewHolder.Callback() {

            @Override
            public void didThemeColorChanged(int color) {
                setThemeColor(color);
            }

            @Override
            public void onPageAttached(@NonNull Tab page) {
                ArkLogger.e(TabSwitcherManager.this, "onPageAttached");
                mBottomController.onPageAttached(page);
                setThemeColor(page.getThemeColor());
            }

            @Override
            public void onPageDetached(@NonNull Tab page) {
                mBottomController.onPageDetached(page);
            }

            @Override
            public void onShutDown() {
                TabGroupManager.global().destroy();
            }
        });
        mViewHolder.onStart();
    }

    public BottomController getBottomController() {
        return mBottomController;
    }

    public SwitcherRecyclerLayout getSwitcher() {
        return mSwitcher;
    }

    public ArkCompositorViewHolder getCompositorViewHolder() {
        return mViewHolder;
    }

    public View getBrowserLayout() {
        return mBrowserLayout;
    }

    public ArkLauncherLayout getLauncherLayout() {
        return mLauncherLayout;
    }

    public TabSwitcherLayout getTabSwitcherLayout() {
        return mTabSwitcherLayout;
    }

    public void showSwitcher() {
        mLauncherLayout.setVisibility(View.VISIBLE);
        hideBrowser();
        mTabSwitcherLayout.showSwitcher();
    }

    public void showBrowser() {
        mBrowserLayout.setVisibility(View.VISIBLE);
        Tab tab = mViewHolder.getTab();
        if (tab != null) {
            int color = tab.getThemeColor();
            setThemeColor(color);
        } else {
            setThemeColor(AppConfig.isNightMode() ? Color.BLACK : Color.WHITE);
        }
    }

    public void hideBrowser() {
        mBrowserLayout.setVisibility(View.INVISIBLE);
        StatusBarUtils.setLightMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
        mViewHolder.setTab(null);
    }

    public void setThemeColor(int color) {
        if (mBrowserLayout.getVisibility() != View.VISIBLE) {
            return;
        }
        mBottomController.updatePrimaryColor(color);
        mBrowserLayout.setBackgroundColor(color);

        boolean useLight = ColorUtils.shouldUseLightForegroundOnBackground(color);
        mTabSwitcherLayout.setStatusBarColor(color);
        Log.d("LauncherManager", "setStatusBarColor color=" + color);
        if (!useLight && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            StatusBarUtils.setDarkMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
        } else {
            StatusBarUtils.setLightMode(ContextUtils.getActivity(mBrowserLayout.getContext()));
        }
    }

    public void onRestore() {
        mSwitcher.setTabGroup(TabGroupManager.global().getCurrentTabGroup());
        mTabSwitcherLayout.onRestore();
    }

    @Override
    public void onExpand(int position) {
        goToBrowser(false);
    }

    @Override
    public void onBeforeIdle(int position) {
        showSwitcher();
        mLauncherLayout.setVisibility(View.INVISIBLE);
    }

    @Override
    public void onHide(int position) {
        goToLauncher();
    }

    @Override
    public void onOpen(float percent) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mLauncherLayout.setAlpha(1 - percent);
    }

    @Override
    public void onClose(float percent) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mLauncherLayout.setAlpha(percent);
    }

    private boolean isVisible(View view) {
        return view.getVisibility() == View.VISIBLE;
    }

    public boolean isInBrowser() {
        return isVisible(mBrowserLayout);
    }

    public boolean isInLauncher() {
        return isVisible(mLauncherLayout) && !isVisible(mTabSwitcherLayout);
    }

    public boolean isInTabSwitcher() {
        return isVisible(mTabSwitcherLayout) && isVisible(mTabSwitcherLayout.getSwitcher());
    }

    public void goToBrowser(boolean animated) {
        mLauncherLayout.setVisibility(View.INVISIBLE);
        showBrowser();
        mTabSwitcherLayout.showBrowser();
    }

    public void goToBrowser() {
        goToBrowser(true);
    }

    public void goToTabSwitcher() {
        cacheCurrentPage();
        mTabSwitcherLayout.showSwitcher();
        mTabSwitcherLayout.open();
        hideBrowser();
    }

    public void goToLauncher(boolean animated) {
        mLauncherLayout.setVisibility(View.VISIBLE);
        mTabSwitcherLayout.setVisibility(View.INVISIBLE);
    }

    public void goToLauncher() {
        goToLauncher(true);
    }

    public boolean onBackPressed() {
        if (isInTabSwitcher()) {
            mSwitcher.close();
            return true;
        } else if (isInBrowser()) {
            if (mViewHolder != null && mViewHolder.onBackPressed()) {
                return true;
            }
            goToTabSwitcher();
            return true;
        }
//        else if (!isInLauncher()) {
//            goToLauncher();
//            return true;
//        }
        return false;
    }

    public void transitionToBrowser(Rect startRect, Runnable endRunnable) {

        mTabSwitcherLayout.setVisibility(View.VISIBLE);
        mSwitcher.setVisibility(View.INVISIBLE);
//        mSwitcherBottomBar.setVisibility(INVISIBLE);
//        mSwitcherTopBar.setVisibility(INVISIBLE);

        hideBrowser();

        Rect endRect = new Rect(0, 0, mTabSwitcherLayout.getWidth(), mTabSwitcherLayout.getHeight());
        ValueAnimator animator = ValueAnimator.ofFloat(0, 1f);

//        layout(startRect.left, startRect.top, startRect.right, startRect.bottom);

        animator.setInterpolator(new DecelerateInterpolator(2));
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float percent = (float) animation.getAnimatedValue();

                int left = (int) (startRect.left + (endRect.left - startRect.left) * percent);
                int top = (int) (startRect.top + (endRect.top - startRect.top) * percent);
                int right = (int) (startRect.right + (endRect.right - startRect.right) * percent);
                int bottom = (int) (startRect.bottom + (endRect.bottom - startRect.bottom) * percent);

                float scale = (startRect.width() + (endRect.width() - startRect.width()) * percent) / endRect.width();

                mBrowserLayout.setPivotX(0);
                mBrowserLayout.setPivotY(0);
                mBrowserLayout.setScaleX(scale);
                mBrowserLayout.setScaleY(scale);

                mBrowserLayout.setTranslationX(left);
                mBrowserLayout.setTranslationY(top);
//                mBrowserLayout.layout(left, top, right, bottom);


                showBrowser();
            }
        });
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mLauncherLayout.setVisibility(View.INVISIBLE);
                if (endRunnable != null) {
                    endRunnable.run();
                }
            }
        });
        animator.setDuration(360);
        animator.start();
    }

    public void onStart() {
        if (mViewHolder != null) {
            mViewHolder.onStart();
        }
    }

    public void onStop() {
        if (mViewHolder != null) {
            mViewHolder.onStop();
        }
    }

    public void onPause() {
        mLauncherLayout.onPause();
    }

    public void onDestroy() {
        mLauncherLayout.onDestroy();
    }

    public void onSaveInstanceState(Bundle outState) {
        mLauncherLayout.onSaveInstanceState(outState);
    }

    public void cacheCurrentPage() {
        PageInfo pageInfo = getCurrentTabGroup().getCurrentPageInfo();
        ArkLogger.e(this, "cacheCurrentPage pageInfo=" + pageInfo);
        PageSnapshotManager.getInstance().cachePage(pageInfo);
        if (pageInfo != null) {
            ArkWebContents arkWeb = ArkWebManager.get(pageInfo.getId());
            if (arkWeb != null) {
                mViewHolder.getTabContentManager().cacheThumbnail(arkWeb.getWebContents(), arkWeb.getId());
            }
        }
    }

    public void openUrl(LoadUrlEvent event) {
//        ITabGroup tabGroup = TabGroupManager.global().getCurrentTabGroup(event.isIncognito());
        ITabGroup tabGroup = getCurrentTabGroup();
        if (tabGroup.isIncognito() != event.isIncognito()) {
            tabGroup = TabGroupManager.global().getCurrentTabGroup(event.isIncognito());
        }
        LoadUrlParams loadUrlParams = event.getLoadUrlParams();
        if (event.isNewTab() || tabGroup.getCurrentTab() == null) {
            loadUrlParams.setTransitionType(PageTransition.GENERATED);
            tabGroup.openInNewTab(event.getPageInfo(), loadUrlParams, TabLaunchType.FROM_CHROME_UI);
        } else if (isInBrowser() && tabGroup.getCurrentTab() != null) {
            // TODO new tab or new page?
            tabGroup.openInNewTab(loadUrlParams, TabLaunchType.FROM_CHROME_UI);
        } else {
            tabGroup.openInNewTab(event.getPageInfo(), loadUrlParams, TabLaunchType.FROM_CHROME_UI);
        }
        TabGroupManager.global().selectGroup(tabGroup);
        goToBrowser();
    }

    private ITabGroup getCurrentTabGroup() {
        ITabGroup tabGroup = mSwitcher.getTabGroup();
        if (tabGroup == null) {
            return TabGroupManager.global().getCurrentTabGroup();
        }
        return tabGroup;
    }


}
