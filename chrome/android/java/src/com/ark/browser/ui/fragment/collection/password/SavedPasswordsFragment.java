package com.ark.browser.ui.fragment.collection.password;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.ark.browser.ui.fragment.collection.CollectionChildFragment;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.KeyguardUtil;
import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.SelectableRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.password_manager.settings.PasswordUIView;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.SavedPasswordEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

public class SavedPasswordsFragment extends CollectionChildFragment
        implements View.OnClickListener,
        IEasy.OnBindViewHolderListener<SavedPasswordEntry>,
        PasswordUIView.Observer {

    private static final String TAG = "OfflinePageFragment";

    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    // The key for saving |mExportRequested| to instance bundle.
    private static final String SAVED_STATE_EXPORT_REQUESTED = "saved-state-export-requested";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";

    private SelectableRecycler<SavedPasswordEntry> mRecycler;

    private TextView editBtn;
    private TextView selectAllBtn;
    private TextView deleteBtn;

    private RoundedIconGenerator mIconGenerator;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private int mCornerRadius;

    private LargeIconBridge mLargeIconBridge;

    private boolean copyPassword = false;
    private String tempCopyStr = "";

    private PasswordUIView mPasswordUIView;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPasswordUIView = new PasswordUIView();
        mPasswordUIView.setObserver(this);

        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min((activityManager.getMemoryClass() / 4) * 1024 * 1024, 10 * 1024 * 1024);
        mLargeIconBridge.createCache(maxSize);

        mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        mMinIconSize = (int) getResources().getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                mDisplayedIconSize / 2,
                iconColor, textSize);
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        mRecycler = new SelectableRecycler<SavedPasswordEntry>(mRecyclerView)
                .addItemDecoration(new ShadowItemDecoration())
                .setOnSelectChangeListener(new IEasy.OnSelectChangeListener<SavedPasswordEntry>() {
                    @Override
                    public void onSelectModeChange(boolean selectMode) {

                    }

                    @Override
                    public void onSelectChange(List<SavedPasswordEntry> list, int position, boolean isChecked) {
                        setBottomBarState(mRecycler.getSelectedCount());
                    }

                    @Override
                    public void onSelectAll() {
                        selectAllBtn.setText("全不选");
                    }

                    @Override
                    public void onUnSelectAll() {
                        selectAllBtn.setText("全选");
                    }

                    @Override
                    public void onSelectOverMax(int maxSelectCount) {

                    }
                })
                .setItemRes(R.layout.item_collection)
                .onBindViewHolder(this)
                .build();
        mRecycler.showLoading();
    }

    @Override
    public void onLazyInitView(@Nullable Bundle savedInstanceState) {
        super.onLazyInitView(savedInstanceState);

        mPasswordUIView.updatePasswordLists();
    }

//    @Subscribe
//    public void onRefreshEvent(RefreshEvent event) {
//        rebuildPasswordLists();
//    }


    @Override
    public void onDestroyView() {
        mPasswordUIView.destroy();
        mPasswordUIView = null;
        super.onDestroyView();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (ReauthenticationManager.authenticationStillValid(ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
            if (copyPassword && !TextUtils.isEmpty(tempCopyStr)) {
                copyPassword(tempCopyStr);
            }
        }
    }

//    public void onEnterAnimationEnd() {
//
//    }

    @Override
    public boolean onBackPressedSupport() {
        if ("完成".equals(editBtn.getText().toString())) {
            editBtn.performClick();
            return true;
        }
        return super.onBackPressedSupport();
    }

//    @Override
//    protected void onRefreshEvent() {
////        rebuildPasswordLists();
//        PasswordManagerHandlerProvider.getInstance()
//                .getPasswordManagerHandler()
//                .updatePasswordLists();
//    }

    @Override
    public void onClick(View v) {
        if (v == editBtn) {
            if (mRecycler.isEmpty()) {
                return;
            }
            String text = editBtn.getText().toString();
            if ("编辑".equals(text)) {
                setBottomBarState(mRecycler.getSelectedCount());
                editBtn.setText("完成");
                selectAllBtn.setVisibility(View.VISIBLE);
                deleteBtn.setVisibility(View.VISIBLE);
                mRecycler.enterSelectMode();
                selectAllBtn.setText("全选");
            } else {
                editBtn.setText("编辑");
                selectAllBtn.setVisibility(View.INVISIBLE);
                deleteBtn.setVisibility(View.INVISIBLE);
                mRecycler.exitSelectMode();
            }
        } else if (v == selectAllBtn) {
            String text = selectAllBtn.getText().toString();
            if (text.equals("全选")) {
                selectAllBtn.setText("全不选");
                mRecycler.selectAll();
            } else {
                selectAllBtn.setText("全选");
                mRecycler.unSelectAll();
            }
            setBottomBarState(mRecycler.getSelectedCount());
        } else if (v == deleteBtn) {
            ZDialog.alert()
                    .setTitle("确定删除？")
                    .setContent("你将删除" + mRecycler.getSelectedCount() + "个书签")
                    .setPositiveButton((fragment, which) -> {
                        ZToast.normal("TODO 删除");
                    })
                    .show(context);
        }
    }

    public void setBottomBarState(int selectCount) {
        if (selectCount != 0) {
            deleteBtn.setTextColor(ApiCompatibilityUtils.getColor(getResources(), R.color.light_red));
            deleteBtn.setClickable(true);
            deleteBtn.setText(getResources().getString(R.string.text_delete_with_count, selectCount));
        } else {
            int color = ApiCompatibilityUtils.getColor(getResources(), R.color.google_grey_400);
            deleteBtn.setTextColor(color);
            deleteBtn.setClickable(false);
            deleteBtn.setText(getResources().getString(R.string.text_delete));
        }
    }

    @Override
    public void passwordListAvailable(int count) {
        ThreadPool.executeIO(() -> {
            List<SavedPasswordEntry> items = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                SavedPasswordEntry saved = mPasswordUIView.getSavedPasswordEntry(i);
                saved.setId(i);
                items.add(saved);
            }
            postOnEnterAnimationEnd(() -> {
                mRecycler.setItems(items);
                mRecycler.notifyDataSetChanged();
            });
        });
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        // TODO 一律不保存
        for (int i = 0; i < count; i++) {
            String exception = mPasswordUIView.getSavedPasswordException(i);
            ArkLogger.e(TAG, "passwordExceptionListAvailable i=" + i + " exception=" + exception);
        }
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<SavedPasswordEntry> list, int position, List<Object> payloads) {
        SavedPasswordEntry item = list.get(position);
        holder.setVisible(R.id.layout_content, true);
        String url = item.getUrl();
        holder.setText(R.id.tv_title, item.getUrl());
        holder.setText(R.id.tv_info, item.getUserName());

        mLargeIconBridge.getLargeIconForUrl(new GURL(url), mMinIconSize, new LargeIconBridge.LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(@Nullable Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) {
                if (icon == null) {
                    holder.setImageDrawable(R.id.iv_icon, new BitmapDrawable(getResources(), mIconGenerator.generateIconForUrl(item.getUrl())));
                } else {
                    RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                            getResources(),
                            Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
                    roundedIcon.setCornerRadius(mCornerRadius);
                    holder.setImageDrawable(R.id.iv_icon, roundedIcon);
                }
            }
        });

        holder.setOnItemClickListener(v -> {
            PasswordEntryEditor editor = PasswordEntryEditor.newInstance(item);
            editor.setRemoveRunnable(() -> {
                mRecycler.showLoading();
                mPasswordUIView.removeSavedPasswordEntry(item.getId());
                mPasswordUIView.updatePasswordLists();
            });
            _mActivity.start(editor);
        });

        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
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
                            if (!ReauthenticationManager.isScreenLockSetUp(getActivity())) {
                                ZToast.warning("要想在此处查看或复制您的密码，请在此设备上设置屏幕锁定。");
                            } else if (ReauthenticationManager.authenticationStillValid(ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
                                copyPassword(item.getPassword());
                            } else {
                                tempCopyStr = item.getPassword();
                                copyPassword = true;
                                KeyguardUtil.with(getActivity()).lockDevice();
                            }
                            break;
                        case 3:
                            ZDialog.alert()
                                    .setTitle("确定删除?")
                                    .setContent("你将删除" + item.getUrl() + "保存的密码")
                                    .setPositiveButton((fragment1, which) -> {
                                        mRecycler.showLoading();
                                        mPasswordUIView.removeSavedPasswordEntry(item.getId());
                                        mPasswordUIView.updatePasswordLists();
                                    })
                                    .show(context);
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
    }

    private void copyPassword(String password) {
        Clipboard.getInstance().setTextFromUser(password);
        tempCopyStr = "";
        copyPassword = false;
        ZToast.success("已复制密码");
    }

    @Override
    public View onCreateBottomBar(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_collection_bottom_bar, null, false);
        selectAllBtn = view.findViewById(R.id.btn_select_all);
        deleteBtn = view.findViewById(R.id.btn_delete);
        editBtn = view.findViewById(R.id.btn_edit);
        selectAllBtn.setOnClickListener(this);
        deleteBtn.setOnClickListener(this);
        editBtn.setOnClickListener(this);
        return view;
    }

    @Override
    public void onShowMenu(View view) {
        ZDialog.attach()
                .addItems("清空")
                .setOnSelectListener((fragment, position, text) -> {
                    ZToast.normal(text);
                    switch (position) {
                        case 0:
                            ZDialog.alert()
                                    .setTitle("确定清空书签？")
                                    .setContent("您将清空所有的书签！！！")
                                    .show(context);
                            break;
                    }
                    fragment.dismiss();
                })
                .setAttachView(view)
                .show(context);
    }

}

