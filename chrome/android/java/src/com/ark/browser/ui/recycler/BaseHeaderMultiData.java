package com.ark.browser.ui.recycler;

import android.view.View;

import com.ark.browser.ui.widget.TitleHeaderLayout;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.ExpandableMultiData;

import org.chromium.chrome.R;

import java.util.List;

public abstract class BaseHeaderMultiData<T> extends ExpandableMultiData<T>
        implements View.OnClickListener {

    protected final String title;

    public BaseHeaderMultiData(String title) {
        super();
        this.title = title;
    }

    public BaseHeaderMultiData(String title, List<T> list) {
        super(list);
        this.title = title;
    }

    @Override
    public int getHeaderLayoutId() {
        return R.layout.item_header_title;
    }

    @Override
    public void onBindHeader(EasyViewHolder holder, List<Object> payloads) {
        TitleHeaderLayout headerLayout = holder.getView(R.id.layout_title_header);
        headerLayout.setTitle(title + "(" + mData.size() + ")");
        headerLayout.setMoreText(null);
        headerLayout.setOnMoreClickListener(showMoreButton() ? this : null);
        holder.setOnItemClickListener(v -> {
            if (isExpand()) {
                collapse();
            } else {
                expand();
            }
        });
    }

    @Override
    public final void onClick(View v) {
        onHeaderClick();
    }

    protected boolean showMoreButton() {
        return true;
    }

    public void onHeaderClick() {

    }

    public void onResume() {

    }

    public void onDestroy() {

    }

}

