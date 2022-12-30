package com.ark.browser.ui.fragment.manager.adblock;

import android.os.Bundle;
import android.text.TextUtils;
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

public class AdblockDomRuleFragment extends BaseSwipeBackFragment {

    private final static String KEY = "url";
    private EasyRecycler<AdblockPlusHelper.MarkAsAd> mRecycler;

    private String mHost;

    public static AdblockDomRuleFragment newInstance(String url) {
        Bundle args = new Bundle();
        args.putString(KEY, url);
        AdblockDomRuleFragment fragment = new AdblockDomRuleFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_script;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        if (getArguments() != null) {
            mHost = getArguments().getString(KEY);
        }
        if (TextUtils.isEmpty(mHost)) {
            pop();
            return;
        }
        setToolbarTitle("DOM规则");
        mRecycler = new EasyRecycler<>(view.findViewById(R.id.recycler_view));
        mRecycler.setItemRes(R.layout.item_dom_rules)
                .onBindViewHolder((holder, list, position, payloads) -> {
                    TextView website = holder.getView(R.id.text_website);
                    TextView tvRule = holder.getView(R.id.text_rule);
                    AdblockPlusHelper.MarkAsAd markAsAd = list.get(position);
                    website.setText(markAsAd.host);
                    tvRule.setText(markAsAd.getRule());
                    holder.setOnItemClickListener(v -> showEditDialog(markAsAd));

                    ClickHelper.with(holder.getItemView())
                            .setOnLongClickListener((v, x, y) -> {
                                ZDialog.attach()
                                        .addItems("编辑", "删除")
                                        .setOnSelectListener((fragment, position1, title) -> {
                                            switch(position1) {
                                                case 0:
                                                    showEditDialog(markAsAd);
                                                    break;
                                                case 1:
                                                    ZDialog.alert()
                                                            .setTitle("确定删除？")
                                                            .setContent("您将删除广告拦截规则：" + markAsAd.getRule())
                                                            .setPositiveButton((fragment1, which) -> {
                                                                ZToast.normal("TODO 删除广告拦截规则");
                                                                AdblockPlusHelper.deleteAdElementsByHost(context, list.get(position), new Runnable() {
                                                                    @Override
                                                                    public void run() {
                                                                        list.clear();
                                                                        list.addAll(AdblockPlusHelper.getDomRulesByUrl(mHost));
                                                                        mRecycler.notifyDataSetChanged();
                                                                    }
                                                                });
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
            List<AdblockPlusHelper.MarkAsAd> list = AdblockPlusHelper.getDomRulesByUrl(mHost);
            postOnEnterAnimationEnd(() -> {
                mRecycler.setItems(list);
                mRecycler.showContent();
            });
        });
    }

    private void showEditDialog(AdblockPlusHelper.MarkAsAd markAsAd) {
        ZDialog.input()
                .setEmptyable(false)
                .setEditText(markAsAd.toString())
                .setMinLines(2)
                .setMaxLines(5)
                .setHint("请输入域名规则")
                .setTitle("编辑")
                .setPositiveButton((fragment, which) -> {
                    String newRule = fragment.getText();
                    ZToast.success("TODO newRule=" + newRule);
                })
                .show(context);
    }
}
