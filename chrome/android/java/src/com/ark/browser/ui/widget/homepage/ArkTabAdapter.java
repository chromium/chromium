package com.ark.browser.ui.widget.homepage;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.cardview.widget.CardView;

import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.FaviconUtil;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

import java.util.List;

public class ArkTabAdapter implements Adapter {

    private final TabContentManager mTabContentManager;
    final List<ITab> tabList = TabGroupManager.global().getCurrentTabGroup().getTabList();

    public ArkTabAdapter(TabContentManager tabContentManager) {
        this.mTabContentManager = tabContentManager;
    }

    @Override
    public View onCreateViewHolder(ViewGroup parent, int position) {
        View itemView = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_tab, parent, false);
        return itemView;
    }

    @Override
    public void onBindViewHolder(View itemView, int position) {
        if (TabGroupManager.global().isLoaded()) {
            ITab tab = tabList.get(position);
            updateItem(itemView, tab, null);
        }
    }

    @Override
    public int getCount() {
        if (TabGroupManager.global().isLoaded()) {
            return tabList.size();
        }
        return 0;
    }

    @Override
    public int getPosition() {
        if (TabGroupManager.global().isLoaded()) {
            return TabGroupManager.global().getCurrentTabGroup().getIndex();
        }
        return 0;
    }

    @Override
    public boolean onSwipe(int position) {
        if (TabGroupManager.global().isLoaded()) {
            TabGroupManager.global().getCurrentTabGroup().closeTab(tabList.get(position));
        }
        return false;
    }

    @Override
    public boolean isLocked(int position) {
        if (position < 0 || position > getCount() - 1) {
            return false;
        }
        return tabList.get(position).getTabInfo().isLocked();
    }

    private void updateItem(View view, ITab iTab, Runnable runnable) {
        TextView tvTitle = view.findViewById(R.id.tv_title);
        ImageView ivIcon = view.findViewById(R.id.iv_icon);
        ImageButton btnLock = view.findViewById(R.id.btn_lock);
        ImageButton btnMore = view.findViewById(R.id.btn_more);
        btnLock.setImageResource(iTab.getTabInfo().isLocked()
                ? R.drawable.ic_lock_24dp : R.drawable.ic_lock_open_24dp);
        btnLock.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                iTab.getTabInfo().setLocked(!iTab.getTabInfo().isLocked());
                btnLock.setImageResource(iTab.getTabInfo().isLocked()
                        ? R.drawable.ic_lock_24dp : R.drawable.ic_lock_open_24dp);
            }
        });
        btnMore.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                TabActionDialog.newInstance(iTab.getId()).show(v);
            }
        });
        CardView cardView = view.findViewById(R.id.card_view);

//        ImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
        FitWidthImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
        PageInfo pageInfo = iTab.getCurrentPageInfo();
        if (pageInfo != null) {
            int theme = pageInfo.getThemeColor();
            cardView.setCardBackgroundColor(theme == 0 ? getDefaultThemeColor() : theme);
            tvTitle.setText(pageInfo.getTitle());
//            Tab tab = PageCacheManager.getInstance().findPage(pageInfo);
//            if (tab == null || tab.getFavicon() == null) {
//                ivIcon.setImageResource(R.drawable.default_favicon_white);
//            } else {
//                ivIcon.setImageBitmap(tab.getFavicon());
//            }
            FaviconUtil.with(view.getContext(), pageInfo.getUrl())
                    .setCallback(ivIcon::setImageDrawable)
                    .start();
            if (!(view.getTag() instanceof Integer) || (int) view.getTag() != iTab.getId()) {
                ivThumbnail.setImageBitmap(null);
            }
            mTabContentManager.getTabThumbnailWithCallback(null, pageInfo.getId(), new Callback<Bitmap>() {
                @Override
                public void onResult(Bitmap result) {
                    if (result == null) {
                        PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
                    } else {
                        ivThumbnail.setImageBitmap(result);
                    }
                }
            }, false, false);
        } else {
            cardView.setCardBackgroundColor(getDefaultThemeColor());
            ivThumbnail.setImageBitmap(null);
        }

        view.setTag(iTab.getId());
    }

    public int getDefaultThemeColor() {
        return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
    }

}
