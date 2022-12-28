package com.ark.browser.ui.fragment.collection.offline;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.ui.fragment.collection.CollectionChildFragment;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.SelectableRecycler;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ContextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.base.Clipboard;
import org.chromium.chrome.R;
import org.chromium.url.GURL;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class OfflinePageFragment extends CollectionChildFragment
        implements View.OnClickListener,
        IEasy.OnBindViewHolderListener<OfflinePageItem>,
        Callback<List<OfflinePageItem>>,
        OfflineContentProvider.Observer {

    private static final String TAG = "OfflinePageFragment";
    private final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());

    private SelectableRecycler<OfflinePageItem> mRecycler;

    private OfflinePageBridge offlinePageBridge;

    private TextView editBtn;
    private TextView selectAllBtn;
    private TextView deleteBtn;


//    private CollectionFragment.OnFragmentListener mOnFragmentListener;

    private RoundedIconGenerator mIconGenerator;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private int mCornerRadius;

    private LargeIconBridge mLargeIconBridge;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        offlinePageBridge = OfflinePageBridge.getForProfile(Profile.getLastUsedRegularProfile());

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
        mRecycler = new SelectableRecycler<OfflinePageItem>(mRecyclerView)
                .addItemDecoration(new ShadowItemDecoration())
                .setOnSelectChangeListener(new IEasy.OnSelectChangeListener<OfflinePageItem>() {
                    @Override
                    public void onSelectModeChange(boolean selectMode) {

                    }

                    @Override
                    public void onSelectChange(List<OfflinePageItem> list, int position, boolean isChecked) {
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
//        offlinePageBridge.getAllPages(OfflinePageFragment.this);
    }

    @Override
    public void onLazyInitView(@Nullable Bundle savedInstanceState) {
        super.onLazyInitView(savedInstanceState);
        offlinePageBridge.getAllPages(OfflinePageFragment.this);
    }

    @Override
    public boolean onBackPressedSupport() {
        if ("完成".equals(editBtn.getText().toString())) {
            editBtn.performClick();
            return true;
        }
        return super.onBackPressedSupport();
    }

    @Override
    public void onResult(List<OfflinePageItem> result) {
        Log.d(TAG, "onResult result=" + result);
        postOnEnterAnimationEnd(new Runnable() {
            @Override
            public void run() {
                mRecycler.setItems(result);
                mRecycler.clearSelectedPosition();
                mRecycler.notifyDataSetChanged();
            }
        });
    }

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
                        List<ClientId> items = new ArrayList<>();
                        for (OfflinePageItem item : mRecycler.getSelectedItem()) {
                            items.add(item.getClientId());
                        }
                        offlinePageBridge.deletePagesByClientId(items, result -> offlinePageBridge.getAllPages(OfflinePageFragment.this));
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
    public void onItemsAdded(List<OfflineItem> items) {
//        offlineContentProvider.getAllItems(this);
    }

    @Override
    public void onItemRemoved(ContentId id) {
//        offlineContentProvider.getAllItems(this);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {

    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<OfflinePageItem> list, int position, List<Object> payloads) {
        OfflinePageItem item = list.get(position);
        holder.getView(R.id.layout_content).setVisibility(View.VISIBLE);
        String url = item.getUrl();
        holder.getTextView(R.id.tv_title).setText(item.getTitle());
        holder.getTextView(R.id.tv_info).setText(sdf.format(item.getCreationTimeMs()));
        holder.getTextView(R.id.tv_desc).setText(url);

        mLargeIconBridge.getLargeIconForUrl(new GURL(url), mMinIconSize, (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
            if (icon == null) {
                holder.setImageDrawable(R.id.iv_icon, new BitmapDrawable(getResources(), mIconGenerator.generateIconForUrl(item.getUrl())));
            } else {
                RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                        getResources(),
                        Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
                roundedIcon.setCornerRadius(mCornerRadius);
                holder.setImageDrawable(R.id.iv_icon, roundedIcon);
            }
        });

        holder.setOnItemClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                        url,
                        item.getOfflineId(),
                        LaunchLocation.DOWNLOAD_HOME,
                        (params) -> {
                            if (params == null) return;
                            LoadUrlEvent.post(params, true);
                        },
                        Profile.getLastUsedRegularProfile()
                );
            }
        });
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
                    showMenu(holder, item, x, y);
                    return true;
                });
    }

    private void showMenu(EasyViewHolder holder, OfflinePageItem item, float x, float y) {
        new AttachListDialogFragment<String>()
                .addItems("新窗口打开", "复制链接", "删除", "添加到主页")
                .setOnSelectListener((fragment, position, text) -> {
                    switch (position) {
                        case 0:
                            LoadUrlEvent.post(item.getUrl(), true);
                            break;
                        case 1:
                            Clipboard.getInstance().setTextFromUser(item.getUrl());
                            break;
                        case 2:
                            ZDialog.alert()
                                    .setTitle("确定删除?")
                                    .setContent("你将删除历史记录：" + item.getTitle())
                                    .setPositiveButton((fragment1, which) -> {
                                        offlinePageBridge.deletePage(item.getClientId(), new Callback<Integer>() {
                                            @Override
                                            public void onResult(Integer result) {
                                                offlinePageBridge.getAllPages(OfflinePageFragment.this);
                                            }
                                        });
//                                            offlineContentProvider.removeItem(item.id);
                                    })
                                    .show(context);
                            break;
                        case 3:
                            ZToast.normal(text);
                            break;
                    }
                    fragment.dismiss();
                })
                .setTouchPoint(x, y)
                .show(context);
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

