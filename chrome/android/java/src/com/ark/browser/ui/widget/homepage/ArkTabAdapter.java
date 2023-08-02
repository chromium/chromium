package com.ark.browser.ui.widget.homepage;

import android.content.Context;
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
import com.ark.browser.tab.MultiThumbnailCardProvider;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.ThumbnailProvider;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.dialog.TabActionDialog;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.FaviconUtil;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

public class ArkTabAdapter implements Adapter {

    private final ThumbnailProvider mThumbnailProvider;

    public ArkTabAdapter(Context context, TabContentManager tabContentManager) {
        mThumbnailProvider = new MultiThumbnailCardProvider(context, tabContentManager);
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
        tvTitle.setText(tab.getTitle());
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
                TabActionDialog.newInstance(tab.getId())
                        .setRenameCallback(tvTitle::setText)
                        .show(v);
            }
        });
        CardView cardView = view.findViewById(R.id.card_view);

//        ImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);
        FitWidthImageView ivThumbnail = view.findViewById(R.id.iv_thumbnail);

        PageInfo pageInfo = tab.getCurrentPageInfo();
        if (!(tab instanceof ITabGroup) && pageInfo != null) {
            int theme = pageInfo.getThemeColor();
            cardView.setCardBackgroundColor(theme == 0 ? getDefaultThemeColor() : theme);
            FaviconUtil.with(view.getContext(), pageInfo.getUrl())
                    .setCallback(ivIcon::setImageDrawable)
                    .start();
            Object tabIdTag = view.getTag(R.id.key_tab_id);
            if (!(tabIdTag instanceof Integer) || (int) tabIdTag != tab.getId()) {
                ivThumbnail.setImageBitmap(null);
            }
        } else {
            cardView.setCardBackgroundColor(getDefaultThemeColor());
        }

        mThumbnailProvider.getTabThumbnailWithCallback(tab, null, new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap result) {
                if (result == null && pageInfo != null) {
                    PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);
                } else {
                    ivThumbnail.setImageBitmap(result);
                }
            }
        }, false, false, false);

        view.setTag(R.id.key_tab_id, tab.getId());
        view.setTag(R.id.key_tab_position, position);
    }

    public int getDefaultThemeColor() {
        return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
    }

}
