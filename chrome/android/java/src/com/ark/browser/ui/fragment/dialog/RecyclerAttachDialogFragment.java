package com.ark.browser.ui.fragment.dialog;

import android.graphics.PointF;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.recyclerview.widget.RecyclerView;

import com.zpj.fragmentation.dialog.DialogAnimator;
import com.zpj.fragmentation.dialog.base.AttachDialogFragment;
import com.zpj.fragmentation.dialog.utils.DialogThemeUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class RecyclerAttachDialogFragment<T> extends AttachDialogFragment<RecyclerAttachDialogFragment<T>> {

    protected RecyclerView recyclerView;

    private float cornerRadius;

    private final List<T> items = new ArrayList<>();

    private Adapter<T> mAdapter;

    public RecyclerAttachDialogFragment() {
        cornerRadius = ScreenUtils.dp2px(16);
        mMinWidth = (int) (ScreenUtils.getScreenWidth() / 2.2f);
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout._dialog_layout_attach_impl_list;
    }

    @Override
    protected DialogAnimator onCreateShadowAnimator(FrameLayout flContainer) {
        return null;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mAdapter == null) {
            popThis();
            return;
        }
        if (attachView == null && touchPoint == null) {
            touchPoint = new PointF(0, 0);
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        if (mAdapter == null) {
            popThis();
            return;
        }
        super.initView(view, savedInstanceState);
        int color = DialogThemeUtils.getDialogBackgroundColor(context);
        CardView cardView = findViewById(R.id.cv_container);
        cardView.setRadius(cornerRadius);
        cardView.setCardBackgroundColor(color);

        recyclerView = findViewById(R.id._dialog_recycler_View);
//        EasyRecycler<T> recycler = new EasyRecycler<>(recyclerView, items);
        mAdapter.onRecyclerViewCreated(recyclerView, items);
    }

    public RecyclerAttachDialogFragment<T> setCornerRadius(float cornerRadius) {
        this.cornerRadius = cornerRadius;
        return this;
    }

    public RecyclerAttachDialogFragment<T> setCornerRadiusDp(float cornerRadiusDp) {
        return setCornerRadius(ScreenUtils.dp2px(cornerRadiusDp));
    }

    public RecyclerAttachDialogFragment<T> setItems(List<T> items) {
        this.items.clear();
        this.items.addAll(items);
        return this;
    }

    public RecyclerAttachDialogFragment<T> setItems(T... items) {
        return setItems(Arrays.asList(items));
    }

    public RecyclerAttachDialogFragment<T> addItems(List<T> items) {
        this.items.addAll(items);
        return this;
    }

    public RecyclerAttachDialogFragment<T> addItems(T... items) {
        this.items.addAll(Arrays.asList(items));
        return this;
    }

    public RecyclerAttachDialogFragment<T> addItemsIf(boolean flag, T... items) {
        if (flag) {
            this.items.addAll(Arrays.asList(items));
        }
        return this;
    }

    public RecyclerAttachDialogFragment<T> addItem(T item) {
        this.items.add(item);
        return this;
    }

    public RecyclerAttachDialogFragment<T> addItemIf(boolean flag, T item) {
        if (flag) {
            this.items.add(item);
        }
        return this;
    }

    public RecyclerAttachDialogFragment<T> setOffsetXAndY(int offsetX, int offsetY) {
        this.mOffsetX += offsetX;
        this.mOffsetY += offsetY;
        return this;
    }

    public void setAdapter(Adapter<T> adapter) {
        mAdapter = adapter;
    }

    public interface Adapter<T> {

        void onRecyclerViewCreated(RecyclerView recyclerView, List<T> items);

    }

}
