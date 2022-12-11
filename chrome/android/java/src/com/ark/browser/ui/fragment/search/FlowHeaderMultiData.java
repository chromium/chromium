package com.ark.browser.ui.fragment.search;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.widget.TextView;

import com.ark.browser.model.SearchHistory;
import com.ark.browser.ui.widget.DotSpan;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.SingleTypeMultiData;
import com.zpj.recyclerview.layouter.FlowLayouter;
import com.zpj.recyclerview.layouter.Layouter;
import com.zpj.statemanager.State;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;

import java.util.List;

public class FlowHeaderMultiData extends SingleTypeMultiData<SearchHistory> {

    private FlowLayouter mLayouter;

    private IEasy.OnItemClickListener<SearchHistory> onItemClickListener;
    private IEasy.OnItemLongClickListener<SearchHistory> onItemLongClickListener;

    public FlowHeaderMultiData() {
        super();
    }

    public void setData(List<SearchHistory> dataSet) {
        mLayouter = null;
        this.mData.clear();
        this.mData.addAll(dataSet);
        if (this.mData.isEmpty()) {
            setState(State.STATE_EMPTY);
        } else {
            setState(State.STATE_CONTENT);
        }
    }

    @Override
    public Layouter getLayouter() {
        if (mLayouter == null) {
            mLayouter = new FlowLayouter(20, 10);
        }
        return mLayouter;
    }

    public SearchHistory removeAt(int index) {
        return this.mData.remove(index);
    }

    @Override
    public boolean loadData() {
        return false;
    }

    @Override
    public int getLayoutId() {
        return R.layout.item_flow;
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<SearchHistory> list, int position, List<Object> payloads) {
        SearchHistory data = list.get(position);
        TextView tvText = holder.getView(R.id.tv_text);
//        int randomColor = getResources().getColor(R.color.color_text_major);
        int randomColor = Color.LTGRAY;
        GradientDrawable drawable = new GradientDrawable();
        drawable.setCornerRadius(ScreenUtils.dp2pxInt(8));
        DotSpan span = new DotSpan(ScreenUtils.dp2pxInt(8), randomColor);
        tvText.setTextColor(Color.BLACK);
        drawable.setStroke(2, randomColor);
        drawable.setColor(Color.WHITE);

        tvText.setBackground(drawable);

        SpannableString spannableString = new SpannableString(data.getText());
        spannableString.setSpan(span, 0, spannableString.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        tvText.setText(spannableString);

        holder.setOnItemClickListener(v -> {
            if (onItemClickListener != null) {
                onItemClickListener.onClick(holder, v, data);
            }
        });

        holder.setOnItemLongClickListener(v -> {
            if (onItemLongClickListener != null) {
                return onItemLongClickListener.onLongClick(holder, v, data);
            }
            return false;
        });
    }

    public void setOnItemClickListener(IEasy.OnItemClickListener<SearchHistory> onItemClickListener) {
        this.onItemClickListener = onItemClickListener;
    }

    public void setOnItemLongClickListener(IEasy.OnItemLongClickListener<SearchHistory> onItemLongClickListener) {
        this.onItemLongClickListener = onItemLongClickListener;
    }

}

