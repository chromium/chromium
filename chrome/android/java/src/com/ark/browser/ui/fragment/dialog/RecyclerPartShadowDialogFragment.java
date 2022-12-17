package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.zpj.fragmentation.dialog.base.PartShadowDialogFragment;
import com.zpj.fragmentation.dialog.enums.DialogPosition;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.skin.SkinEngine;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class RecyclerPartShadowDialogFragment extends PartShadowDialogFragment<RecyclerPartShadowDialogFragment>
        implements IEasy.OnBindViewHolderListener<String> {

    public interface OnItemClickListener {
        void onItemClick(View view, String title, int position);
    }

    protected EasyRecycler<String> mRecycler;
    protected OnItemClickListener onItemClickListener;
    protected int selectPosition = 0;
    protected final List<String> items = new ArrayList<>();

    @Override
    protected int getContentLayoutId() {
        return R.layout.fragment_dialog_part_shadow_recycler;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        dialogPosition = DialogPosition.Bottom;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mRecycler = new EasyRecycler<>(findViewById(R.id.recycler_view));
        mRecycler.setItems(items)
                .setItemRes(R.layout.item_text)
                .onItemClick((holder, view1, data) -> {
                    int position = holder.getRealPosition();
                    if (position == selectPosition) {
                        return;
                    }
                    int old = selectPosition;
                    selectPosition = position;
                    mRecycler.notifyItemChanged(old);
                    mRecycler.notifyItemChanged(selectPosition);
                    dismiss();
                    if (onItemClickListener != null) {
                        onItemClickListener.onItemClick(view1, items.get(position), position);
                    }
                })
                .onBindViewHolder(this);
        buildRecycler(mRecycler);
        mRecycler.build();
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<String> list, int position, List<Object> payloads) {
        TextView title = holder.getTextView(R.id.tv_title);
        title.setText(list.get(position));
        int primaryColor = getResources().getColor(R.color.colorPrimary);
        title.setTextColor(position == selectPosition ? primaryColor : SkinEngine.getColor(context, R.attr.textColorNormal));
    }

    @Override
    protected void onCancel() {
        super.onCancel();
        super.onDismiss();
    }

    protected void buildRecycler(EasyRecycler<String> recycler) {

    }

    public RecyclerPartShadowDialogFragment setOnItemClickListener(OnItemClickListener onItemClickListener) {
        this.onItemClickListener = onItemClickListener;
        return this;
    }

    public RecyclerPartShadowDialogFragment setSelectedItem(int position) {
        this.selectPosition = position;
        return this;
    }

    public RecyclerPartShadowDialogFragment addItemIf(String item, boolean canAdd) {
        if (canAdd) {
            this.items.add(item);
        }
        return this;
    }

    public RecyclerPartShadowDialogFragment addItem(String item) {
        this.items.add(item);
        return this;
    }

    public RecyclerPartShadowDialogFragment addItems(String...items) {
        this.items.addAll(Arrays.asList(items));
        return this;
    }

    public RecyclerPartShadowDialogFragment addItems(List<String> items) {
        this.items.addAll(items);
        return this;
    }

}

