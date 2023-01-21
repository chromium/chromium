package com.ark.browser.ui.recycler;

import android.text.TextUtils;

import com.ark.browser.ui.fragment.collection.password.PasswordEntryEditor;
import com.ark.browser.utils.FaviconUtil;
import com.ark.browser.utils.KeyguardUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.statemanager.State;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.password_manager.settings.PasswordUIView;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.SavedPasswordEntry;
import org.chromium.ui.base.Clipboard;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class SavedPasswordMultiData extends BaseHeaderMultiData<SavedPasswordEntry>
        implements PasswordUIView.Observer {

    private static final String TAG = "BookmarkMultiData";

    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private PasswordUIView mPasswordUIView;

    private boolean copyPassword = false;
    private String tempCopyStr = "";

    private String keyword;

    public SavedPasswordMultiData(String keyword) {
        super("保存密码");
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    public SavedPasswordMultiData(String keyword, List<SavedPasswordEntry> list) {
        super("保存密码", list);
        this.keyword = keyword;
        setState(State.STATE_LOADING);
        init();
    }

    private void init() {
        mPasswordUIView = new PasswordUIView();
        mPasswordUIView.setObserver(this);
    }

    @Override
    public int getChildViewType(int position) {
        return R.layout.item_collection;
    }

    @Override
    public int getChildLayoutId(int viewType) {
        return R.layout.item_collection;
    }

    @Override
    public void onBindChild(EasyViewHolder holder, List<SavedPasswordEntry> list, int position, List<Object> payloads) {
        SavedPasswordEntry item = list.get(position);
        holder.setVisible(R.id.layout_content, true);
        String url = item.getUrl();
        holder.setText(R.id.tv_title, item.getUrl());
        holder.setText(R.id.tv_info, item.getUserName());

        FaviconUtil.with(holder.getContext(), url)
                .setCallback(result -> holder.setImageDrawable(R.id.iv_icon, result))
                .start();

        holder.setOnItemClickListener(v -> {
            PasswordEntryEditor editor = PasswordEntryEditor.newInstance(item);
            editor.setRemoveRunnable(() -> {
                showLoading();
                mPasswordUIView.removeSavedPasswordEntry(item.getId());
                mPasswordUIView.updatePasswordLists();
            });
            editor.show(holder.getContext());
        });

        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
    }

    @Override
    public boolean loadData() {
        mPasswordUIView.updatePasswordLists();
        return false;
    }

    @Override
    public void passwordListAvailable(int count) {
        ThreadPool.executeIO(() -> {
            List<SavedPasswordEntry> items = new ArrayList<>();
            String key = keyword.toLowerCase();
            for (int i = 0; i < count; i++) {
                SavedPasswordEntry saved = mPasswordUIView.getSavedPasswordEntry(i);
                saved.setId(i);

                if (saved.getUrl().toLowerCase().contains(key)
                        || saved.getUserName().toLowerCase().contains(key)) {
                    items.add(saved);
                }


            }

            getAdapter().post(() -> {
                mData.clear();
                mData.addAll(items);
                showContent();
            });
        });
    }

    @Override
    public void passwordExceptionListAvailable(int count) {

    }

    @Override
    public void onResume() {
        if (ReauthenticationManager.authenticationStillValid(ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
            if (copyPassword && !TextUtils.isEmpty(tempCopyStr)) {
                copyPassword(tempCopyStr);
            }
        }
    }

    @Override
    public void onDestroy() {
        if (mPasswordUIView != null) {
            mPasswordUIView.destroy();
            mPasswordUIView = null;
        }
    }

    private void showMenu(EasyViewHolder holder, SavedPasswordEntry item, float x, float y) {
        ZDialog.attach()
                .addItems("复制链接", "复制用户名", "复制密码", "删除")
                .setOnSelectListener((fragment, position, title) -> {
                    switch (position) {
                        case 0:
                            Clipboard.getInstance().setTextFromUser(item.getUrl());
                            break;
                        case 1:
                            Clipboard.getInstance().setTextFromUser(item.getUserName());
                            break;
                        case 2:
                            if (!ReauthenticationManager.isScreenLockSetUp(holder.getContext())) {
                                ZToast.warning("要想在此处查看或复制您的密码，请在此设备上设置屏幕锁定。");
                            } else if (ReauthenticationManager.authenticationStillValid(ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                                copyPassword(item.getPassword());
                            } else {
                                tempCopyStr = item.getPassword();
                                copyPassword = true;
                                KeyguardUtil.with(ContextUtils.getActivity(holder.getContext())).lockDevice();
                            }
                            break;
                        case 3:
                            ZDialog.alert()
                                    .setTitle("确定删除?")
                                    .setContent("你将删除" + item.getUrl() + "保存的密码")
                                    .setPositiveButton((fragment1, which) -> {
                                        showLoading();
                                        mPasswordUIView.removeSavedPasswordEntry(item.getId());
                                        mPasswordUIView.updatePasswordLists();
                                    })
                                    .show(holder.getContext());
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(holder.getContext());
    }

    private void copyPassword(String password) {
        Clipboard.getInstance().setTextFromUser(password);
        tempCopyStr = "";
        copyPassword = false;
        ZToast.success("已复制密码");
    }

}

