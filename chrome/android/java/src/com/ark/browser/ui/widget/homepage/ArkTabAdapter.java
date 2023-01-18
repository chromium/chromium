package com.ark.browser.ui.widget.homepage;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.cardview.widget.CardView;

import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.FaviconUtil;

import org.chromium.chrome.R;

import java.util.List;

public class ArkTabAdapter implements Adapter {

    final List<ITab> tabList = TabListManager.getInstance().getCurrentTabList().getTabList();

    @Override
    public View onCreateViewHolder(ViewGroup parent, int position) {
        View itemView = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_tab, parent, false);
        return itemView;
    }

    @Override
    public void onBindViewHolder(View itemView, int position) {
        if (TabListManager.getInstance().isLoaded()) {
            ITab tab = tabList.get(position);
            updateItem(itemView, tab, null);
        }
    }

    @Override
    public int getCount() {
        if (TabListManager.getInstance().isLoaded()) {
            return tabList.size();
        }
        return 0;
    }

    @Override
    public int getPosition() {
        if (TabListManager.getInstance().isLoaded()) {
            return TabListManager.getInstance().getCurrentTabList().getIndex();
        }
        return 0;
    }

    @Override
    public boolean onSwipe(int position) {
        if (TabListManager.getInstance().isLoaded()) {
            TabListManager.getInstance().getCurrentTabList().closeTab(tabList.get(position));
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
            cardView.setCardBackgroundColor(pageInfo.getThemeColor());
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
            PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
        } else {
            cardView.setCardBackgroundColor(Color.WHITE);
            ivThumbnail.setImageBitmap(null);
        }

    }

}
