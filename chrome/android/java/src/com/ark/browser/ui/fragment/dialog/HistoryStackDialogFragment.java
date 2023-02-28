package com.ark.browser.ui.fragment.dialog;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.pageinfo.PageInfoFragment;
import com.ark.browser.ui.fragment.settings.website.SingleWebsiteFragment;
import com.ark.browser.ui.widget.DialogHeaderLayout;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.zpj.fragmentation.dialog.DialogAnimator;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.recyclerview.IEasy;
import com.zpj.recyclerview.decoration.ShadowItemDecoration;
import com.zpj.toast.ZToast;
import com.zpj.utils.ClickHelper;
import com.zpj.utils.ScreenUtils;
import com.zpj.widget.checkbox.ZCheckBox;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class HistoryStackDialogFragment extends OverDragBottomDialogFragment<HistoryStackDialogFragment>
        implements IEasy.OnBindViewHolderListener<IPage> {

    private static final String KEY_ID = "key_id";

    protected final List<IPage> list = new ArrayList<>();

    private EasyRecycler<IPage> mRecycler;

    private int mSelectPosition = -1;

    private int mThumbnailWidth;
    private int mThumbnailHeight;

    private ValueAnimator mAnimator;

    private ITab mTab;

    public static HistoryStackDialogFragment newInstance(PageInfo pageInfo) {
        return newInstance(pageInfo.getTabId());
    }

    public static HistoryStackDialogFragment newInstance(int tabId) {
        Bundle args = new Bundle();
        args.putInt(KEY_ID, tabId);
        HistoryStackDialogFragment fragment = new HistoryStackDialogFragment();
        fragment.setArguments(args);
        return fragment;
    }

    public static HistoryStackDialogFragment newInstance(Tab page) {
        ITab tab = TabListManager.getInstance().getTabInfo(page);
        Bundle args = new Bundle();
        args.putInt(KEY_ID, tab.getId());
        HistoryStackDialogFragment fragment = new HistoryStackDialogFragment();
        fragment.setArguments(args);
        return fragment;
    }

    public HistoryStackDialogFragment() {
        setMarginTop(ScreenUtils.getStatusBarHeight() * 2);
        setCornerRadiusDp(24);
    }


    @Override
    protected DialogAnimator onCreateDialogAnimator(ViewGroup contentView) {
        return new OverDragDialogAnimator(contentView) {

            @Override
            public Animator onCreateDismissAnimator() {

                if (mAnimator == null) {
                    return super.onCreateDismissAnimator();
                } else {
                    mAnimator.setDuration(getDismissAnimDuration());
                    mAnimator.setInterpolator(AnimationUtils.loadInterpolator(context,
                            R.anim.fast_out_extra_slow_in));
                    return mAnimator;
                }
            }
        };
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int id = 0;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                id = getArguments().getInt(KEY_ID);
            }
        } else {
            id = savedInstanceState.getInt(KEY_ID);
        }
        mTab = TabListManager.getInstance().getTabById(id);
        if (mTab == null) {
            popThis();
        } else {
            list.clear();
            list.addAll(mTab.getPages());
            Collections.reverse(list);
            mSelectPosition = list.size() - mTab.getTabInfo().getIndex() - 1;
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mTab != null) {
            outState.putLong(KEY_ID, mTab.getId());
        }
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_history_stack;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        mAnimator = null;

        mThumbnailWidth = ScreenUtils.getScreenWidth() / 6;
        mThumbnailHeight = (int) (mThumbnailWidth * 1.8f); // ScreenUtils.getScreenHeight() / 6

        DialogHeaderLayout headerLayout = findViewById(R.id.dialog_header);
        headerLayout.setOnCloseClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });
        headerLayout.setTitle("历史栈（" + list.size() + "）");

        RecyclerView recyclerView = findViewById(R.id.recyclerView);
        recyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                super.onScrolled(recyclerView, dx, dy);
            }
        });
        initRecyclerView(recyclerView, list);

        contentView.setMinimumHeight(ScreenUtils.getScreenHeight(context) * 3 / 4);
    }

    @Override
    public void onShowAnimationEnd(Bundle savedInstanceState) {
        super.onShowAnimationEnd(savedInstanceState);
    }

    protected void initRecyclerView(RecyclerView recyclerView, List<IPage> list) {
        this.mRecycler = new EasyRecycler<>(recyclerView, list);
        this.mRecycler.setItemRes(R.layout.item_history_stack)
                .addItemDecoration(new ShadowItemDecoration())
                .onBindViewHolder(this)
                .build();
    }

    @Override
    public void onBindViewHolder(EasyViewHolder holder, List<IPage> list, int position, List<Object> payloads) {
        PageInfo pageInfo = list.get(position).getPageInfo();

//        ImageView ivThumbnail = holder.getView(R.id.iv_thumbnail);
//        ViewGroup.LayoutParams params = ivThumbnail.getLayoutParams();
//        params.width = mThumbnailWidth;
//        params.height = mThumbnailHeight;
//        ivThumbnail.setLayoutParams(params);
//
//        TabThumbnailManager.getInstance().loadTabThumbnail(ivThumbnail, tab);


        FitWidthImageView ivThumbnail = holder.getView(R.id.iv_thumbnail);
        ViewGroup.LayoutParams params = ivThumbnail.getLayoutParams();
        params.width = mThumbnailWidth;
        params.height = mThumbnailHeight;
        ivThumbnail.setLayoutParams(params);
        PageSnapshotManager.getInstance().loadSnapshot(ivThumbnail, pageInfo);

        holder.setText(R.id.tv_title, pageInfo.getTitle());
        holder.setText(R.id.tv_url, pageInfo.getUrl());

        final ZCheckBox checkBox = holder.getView(R.id.check_box);
        checkBox.setOnClickListener(null);
        checkBox.setChecked(mSelectPosition == position, false);
        holder.setOnItemClickListener(v -> {
            if (!checkBox.isChecked()) {
                mRecycler.notifyItemChanged(mSelectPosition);
                mSelectPosition = holder.getAdapterPosition();
                checkBox.setChecked(true, true);
//                TabListManager.getInstance().selectTab(list.get(mSelectPosition).getId());
//                dismiss();

                startAnim(ivThumbnail, pageInfo);

            }
        });
        ClickHelper.with(holder.getItemView())
                .setOnLongClickListener((v, x, y) -> {
//                    PageActionDialog.start(tab, x, y);

                    PageActionDialog popup = PageActionDialog.newInstance(pageInfo.getId());
                    popup.setOnSelectListener((fragment, position1, text) -> {
                        switch(position1){
                            case 0:
                                LoadUrlEvent.post(pageInfo, true, false);
                                break;
                            case 1:
                                LoadUrlEvent.post(pageInfo, true, true);
                                break;
                            case 2:
                                ZToast.normal("TODO 悬浮打开");
                                break;
                            case 3:
                                boolean r = TabListManager.moveToNewTab(pageInfo);
                                if (r) {
                                    ZToast.success("移动页面成功！");
                                    startAnim(ivThumbnail, pageInfo);
                                } else {
                                    ZToast.error("移动页面失败！");
                                }
                                break;
                            case 4:
                                Clipboard.getInstance().setTextFromUser(pageInfo.getTitle());
                                ZToast.success("标题复制成功");
                                break;
                            case 5:
                                Clipboard.getInstance().setTextFromUser(pageInfo.getUrl());
                                ZToast.success("链接复制成功");
                                break;
                            case 6:
                                ArkWebContents arkWeb = ArkWebManager.get(pageInfo.getId());
                                if (arkWeb == null) {
                                    start(SingleWebsiteFragment.newInstance(pageInfo));
                                } else {
                                    start(PageInfoFragment.newInstance(pageInfo.getId()));
                                }
                                break;
                            default:
                                break;
                        }
                        fragment.dismiss();
                    });
                    popup.setTouchPoint(x, y);
                    popup.show(v.getContext());

                    return true;
                });
    }

    private void startAnim(View ivThumbnail, PageInfo pageInfo) {
        Rect startRect = new Rect();
        Rect endRect = new Rect(0, 0, getRootView().getMeasuredWidth(), getRootView().getMeasuredHeight());
        ivThumbnail.getGlobalVisibleRect(startRect);

        CardView cardView = new CardView(context);
        cardView.setCardBackgroundColor(pageInfo.getThemeColor());
        cardView.setCardElevation(0);
        cardView.setRadius(8);
        cardView.setVisibility(View.INVISIBLE);
        cardView.setUseCompatPadding(false);
        getRootView().addView(cardView);

        FitWidthImageView mIvPlaceholder;
        mIvPlaceholder = new FitWidthImageView(context);
//                mIvPlaceholder.setVisibility(View.INVISIBLE);
//                mIvPlaceholder.setScaleType(ImageView.ScaleType.CENTER_CROP);
        cardView.addView(mIvPlaceholder, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        cardView.measure(View.MeasureSpec.makeMeasureSpec(startRect.width(), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(startRect.height(), View.MeasureSpec.EXACTLY));
        cardView.layout(startRect.left, startRect.top, startRect.right, startRect.bottom);

        PageSnapshotManager.getInstance().loadSnapshot(mIvPlaceholder, pageInfo);

        setDismissAnimDuration(300);
        mAnimator = ValueAnimator.ofFloat(0f, 1f);
        mAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float percent = (float) animation.getAnimatedValue();
                int width = (int) ((endRect.width() - startRect.width()) * percent + startRect.width());
                int height = (int) ((endRect.height() - startRect.height()) * percent + startRect.height());
                int left = (int) (startRect.left * (1f - percent));
                int top = (int) (startRect.top * (1f - percent));
                Log.d("HistoryStack", "width=" + width + " height=" + height
                        + " left=" + left + " right=" + (left + width) + " top=" + top + " bottom=" + (top + height));
                cardView.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
                cardView.layout(left, top, left + width, top + height);
//                        mIvPlaceholder.setVisibility(View.VISIBLE);
                cardView.setVisibility(View.VISIBLE);
            }
        });
        mAnimator.addListener(new AnimatorListenerAdapter() {

            @Override
            public void onAnimationStart(Animator animation) {
                TabListManager.getInstance().selectTab(mTab, list.get(mSelectPosition));
            }

            @Override
            public void onAnimationEnd(Animator animation) {
//                ChromeActivity.fromContext(context).getLauncherFragment().getLauncherManager().goToBrowser(false);
                getRootView().setVisibility(View.GONE);
            }
        });
        dismiss();
    }

}
