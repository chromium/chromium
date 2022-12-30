package com.ark.browser.ui.fragment.manager.adblock;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.adblock.AdblockPlusHelper;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;

import org.chromium.chrome.R;

import java.util.List;

public class AdblockDomWebsitesFragment extends BaseSwipeBackFragment {

    private EasyRecycler<String> mRecycler;

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_script;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("DOM规则");
        mRecycler = new EasyRecycler<>(view.findViewById(R.id.recycler_view));
        mRecycler.setItemRes(R.layout.item_domain_rules)
                .onBindViewHolder((holder, list, position, payload) -> {
                    TextView rule = holder.getView(R.id.text_rule);
                    rule.setText(list.get(position));
                    holder.setOnItemClickListener(v -> _mActivity.start(AdblockDomRuleFragment.newInstance(list.get(position))));
                    ClickHelper.with(holder.getItemView())
                            .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
                                @Override
                                public boolean onLongClick(View v, float x, float y) {
                                    ZDialog.attach()
                                            .addItem("删除")
                                            .setOnSelectListener((fragment, position1, title) -> {
                                                if (position1 == 0) {
                                                    ZDialog.alert()
                                                            .setTitle("确定删除？")
                                                            .setContent(String.format("您将删除关于%s网站的所有Dom规则：", list.get(position)))
                                                            .setPositiveButton((fragment1, which) -> {
                                                                ZToast.normal("TODO 删除Dom规则");
                                                                AdblockPlusHelper.deleteAdElementsByHost(context, list.get(position), new Runnable() {
                                                                    @Override
                                                                    public void run() {
                                                                        list.clear();
                                                                        list.addAll(AdblockPlusHelper.getAllRuleHost());
                                                                        mRecycler.notifyDataSetChanged();
                                                                    }
                                                                });
                                                            })
                                                            .show(context);
                                                }
                                                fragment.dismiss();
                                            })
                                            .setTouchPoint(x, y)
                                            .show(context);
                                    return true;
                                }
                            });
                })
                .build();
        mRecycler.showLoading();
        ThreadPool.execute(() -> {
            List<String> list = AdblockPlusHelper.getAllRuleHost();
            postOnEnterAnimationEnd(() -> {
                mRecycler.setItems(list);
                mRecycler.showContent();
            });
        });
    }

    private void showEditDialog(AdblockPlusHelper.MarkAsAd markAsAd) {

    }
}
