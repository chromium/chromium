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

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.FaviconUtil;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.content_public.browser.WebContents;

public class ArkTabAdapter implements Adapter {

    private final TabContentManager mTabContentManager;

    public ArkTabAdapter(TabContentManager tabContentManager) {
        mTabContentManager = tabContentManager;
    }

    @Override
    public View onCreateViewHolder(ViewGroup parent, ITab tab, int position) {
        View itemView = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_tab, parent, false);
        return itemView;
    }

    @Override
    public void onBindViewHolder(View view, ITab tab, int position) {
        TextView tvTitle = view.findViewById(R.id.tv_title);
        ImageView ivIcon = view.findViewById(R.id.iv_icon);
        ImageButton btnLock = view.findViewById(R.id.btn_lock);
        ImageButton btnMore = view.findViewById(R.id.btn_more);
        btnLock.setImageResource(tab.getTabInfo().isLocked()
                ? R.drawable.ic_lock_24dp : R.drawable.ic_lock_open_24dp);
        btnLock.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                tab.getTabInfo().setLocked(!tab.getTabInfo().isLocked());
                btnLock.setImageResource(tab.getTabInfo().isLocked()
                        ? R.drawable.ic_lock_24dp : R.drawable.ic_lock_open_24dp);
            }
        });
        btnMore.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                TabActionDialog.newInstance(tab.getId()).show(v);
            }
        });
        CardView cardView = view.findViewById(R.id.card_view);

//        ImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
        FitWidthImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
        PageInfo pageInfo = tab.getCurrentPageInfo();
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
            Object tabIdTag = view.getTag(R.id.key_tab_id);
            if (!(tabIdTag instanceof Integer) || (int) tabIdTag != tab.getId()) {
                ivThumbnail.setImageBitmap(null);
            }
//            ArkWebContents web = ArkWebManager.get(pageInfo.getId());
//            WebContents webContents = web == null ? null : web.getWebContents();
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

        view.setTag(R.id.key_tab_id, tab.getId());
        view.setTag(R.id.key_tab_position, position);
    }

    public int getDefaultThemeColor() {
        return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
    }

}
