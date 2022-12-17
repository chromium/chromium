package com.ark.browser.ui.fragment.download;

import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ui.fragment.dialog.RecyclerPartShadowDialogFragment;
import com.ark.browser.ui.recycler.FlowLayoutManager;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.skin.SkinEngine;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;

import java.util.List;

public class DownloadFilterDialog extends RecyclerPartShadowDialogFragment {


    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<String> list, int position, List<Object> payloads) {
        TextView title = holder.getTextView(R.id.tv_title);
        title.setText(list.get(position));
        if (position == selectPosition) {
            holder.setTextColor(R.id.tv_title, Color.WHITE);
            holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape_select);
        } else {
            holder.setTextColor(R.id.tv_title, SkinEngine.getColor(context, R.attr.textColorNormal));
            holder.setBackgroundResource(R.id.tv_title, R.drawable.grey_shape2);
        }
    }

    @Override
    protected void buildRecycler(EasyRecycler<String> recycler) {
        int dp8 = ScreenUtils.dp2pxInt(8);
        recycler
                .setItemRes(R.layout.item_download_filter)
                .setLayoutManager(new FlowLayoutManager())
                .addItemDecoration(new RecyclerView.ItemDecoration() {
                    @Override
                    public void getItemOffsets(@NonNull Rect outRect, @NonNull View view, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
                        outRect.top = dp8;
                        outRect.left = dp8;
                        outRect.right = dp8;
                        outRect.bottom = dp8;
                    }
                });
    }
}

