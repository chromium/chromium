package com.ark.browser.ui.fragment.manager.adblock;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.adblock.AdBlock;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.List;

public class AdblockDomainRuleFragment extends BaseSwipeBackFragment {

    private EasyRecycler<String> mRecycler;

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_script;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("域名规则");
        mRecycler = new EasyRecycler<>(view.findViewById(R.id.recycler_view));
        mRecycler.setItemRes(R.layout.item_domain_rules)
                .onBindViewHolder((holder, list, position, payloads) -> {
                    TextView rule = holder.getView(R.id.text_rule);
                    rule.setText(list.get(position));
                    holder.setOnItemClickListener(v -> showEditDialog(rule.getText().toString()));
                    ClickHelper.with(holder.getItemView())
                            .setOnLongClickListener((v, x, y) -> {
                                new AttachListDialogFragment<String>()
                                        .addItems("编辑", "删除")
                                        .setOnSelectListener((fragment, position1, text) -> {
                                            switch (position1) {
                                                case 0:
                                                    showEditDialog(rule.getText().toString());
                                                    break;
                                                case 1:
                                                    ZDialog.alert()
                                                            .setTitle("确定删除？")
                                                            .setContent("您将删除广告拦截规则：" + rule.getText())
                                                            .setPositiveButton((fragment1, which) -> {
                                                                ZToast.success("TODO 删除成功！");
                                                            })
                                                            .show(context);
                                                    break;
                                            }
                                            fragment.dismiss();
                                        })
                                        .setTouchPoint(x, y)
                                        .show(context);
                                return true;
                            });
                })
                .build();
        mRecycler.showLoading();
        ThreadPool.execute(() -> {
            List<String> list = new ArrayList<>(AdBlock.getHosts());
            postOnEnterAnimationEnd(() -> {
                mRecycler.setItems(list);
                mRecycler.showContent();
            });
        });
    }

    private void showEditDialog(String originRule) {
        ZDialog.input()
                .setEmptyable(false)
                .setEditText(originRule)
                .setHint("请输入域名规则")
                .setTitle("编辑")
                .setPositiveButton((fragment, which) -> {
                    String newRule = fragment.getText();
                    ZToast.success("TODO newRule=" + newRule);
                })
                .show(context);
    }
}
