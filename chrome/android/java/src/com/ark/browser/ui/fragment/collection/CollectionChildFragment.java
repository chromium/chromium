package com.ark.browser.ui.fragment.collection;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ui.fragment.base.SkinFragment;

import org.chromium.chrome.R;

public abstract class CollectionChildFragment extends SkinFragment {

    protected RecyclerView mRecyclerView;
    private View mShadowBottomView;
    private View mBottomBar;

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_collections_child;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mRecyclerView = findViewById(R.id.recycler_view);
        mShadowBottomView = findViewById(R.id.view_shadow_bottom);
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                super.onScrolled(recyclerView, dx, dy);
                initShadow(recyclerView);
            }
        });
        initShadow(mRecyclerView);
    }

    private void initShadow(RecyclerView recyclerView) {
        if (mShadowBottomView != null) {

            mShadowBottomView.setVisibility(View.GONE);

//            if (!recyclerView.canScrollVertically(-1)) {
//                mShadowBottomView.setVisibility(View.GONE);
//            } else {
//                mShadowBottomView.setVisibility(View.VISIBLE);
//            }
        }
    }

    @Override
    public final void onEnterAnimationEnd(Bundle savedInstanceState) {
//        mDelegate.onEnterAnimationEnd(savedInstanceState);
//        if (getParentFragment() instanceof SupportFragment) {
//            ((SupportFragment) getParentFragment()).postOnEnterAnimationEnd(new Runnable() {
//                @Override
//                public void run() {
//                    mEnterAnimationEndActionQueue.start();
//                }
//            });
//        }
        super.onEnterAnimationEnd(savedInstanceState);
    }

    public final void onCreateBottomBar(Context context, ViewGroup container) {
        postOnEnterAnimationEnd(new Runnable() {
            @Override
            public void run() {
                if (mBottomBar == null) {
                    mBottomBar = onCreateBottomBar(context);
                }
                if (container.getChildAt(0) == mBottomBar) {
                    return;
                }
                container.removeAllViews();
                container.addView(mBottomBar);
            }
        });
    }

    public abstract View onCreateBottomBar(Context context);

    public abstract void onShowMenu(View view);

}

