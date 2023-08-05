package com.ark.browser.ui.fragment.dialog;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;

import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.settings.Keys;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.core.IPageGroup;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.fragment.settings.website.SingleWebsiteFragment;
import com.zpj.bus.ZBus;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.toast.ZToast;
import com.zpj.utils.Callback;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;

public class TabActionDialog extends AttachListDialogFragment<TabActionDialog.Action>
        implements AttachListDialogFragment.OnSelectListener<TabActionDialog.Action> {

    public abstract static class Action {

        private final String mTitle;

        private Action(String title) {
            mTitle = title;
        }

        abstract void onClick();

    }

    private Callback<String> mRenameCallback;
    private ITab mTab;

    public static TabActionDialog newInstance(ITab tab, float x, float y) {
        TabActionDialog dialog = newInstance(tab.getId());
        dialog.setTouchPoint(x, y);
        return dialog;
    }

    public static TabActionDialog newInstance(int id) {
        Bundle args = new Bundle();
        args.putInt(Keys.KEY_ID, id);
        TabActionDialog fragment = new TabActionDialog();
        fragment.setArguments(args);
        return fragment;
    }

    public TabActionDialog() {
        setOnSelectListener(this);
        onBindTitle((textView, action, i) -> textView.setText(action.mTitle));
        mMinWidth = (int)(ScreenUtils.getScreenWidth() / 2.1F);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int id = Tab.INVALID_PAGE_ID;
        if (savedInstanceState == null) {
            if (getArguments() != null) {
                id = getArguments().getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
            }
        } else {
            id = savedInstanceState.getInt(Keys.KEY_ID, Tab.INVALID_PAGE_ID);
        }
        mTab = TabGroupManager.findTabById(id);
        if (mTab == null) {
            popThis();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mTab != null) {
            outState.putInt(Keys.KEY_ID, mTab.getId());
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        addItem(new Action(mTab.getTabInfo().isLocked() ? "解锁标签" : "锁定标签") {
            @Override
            void onClick() {
                mTab.getTabInfo().setLocked(!mTab.getTabInfo().isLocked());
            }
        });


        addItem(new Action("复制标题") {
            @Override
            void onClick() {
                Clipboard.getInstance().setTextFromUser(mTab.getTitle());
                ZToast.success("标题复制成功");
            }
        });

        boolean isGroup = mTab.getTabInfo().isGroup();
        if (isGroup) {
            if (mRenameCallback != null) {
                addItem(new Action("编辑群组标题") {
                    @Override
                    void onClick() {
                        ZDialog.input()
                                .setHint("请输入群组标题")
                                .setEditText(mTab.getTabInfo().getTitle())
                                .setEmptyable(false)
                                .setAutoShowKeyboard(true)
                                .setTitle("重命名群组标题")
                                .setPositiveButton((fragment, which) -> {
                                    String text = fragment.getText();
                                    mTab.getTabInfo().setTitle(text);
                                    mTab.saveTabInfo();
                                    if (mRenameCallback != null) {
                                        mRenameCallback.onCallback(text);
                                        mRenameCallback = null;
                                    }
                                })
                                .show(context);
                    }
                });
            }
        } else {
            addItem(new Action("复制链接") {
                @Override
                void onClick() {
                    Clipboard.getInstance().setTextFromUser(mTab.getCurrentPageInfo().getUrl());
                    ZToast.success("链接复制成功");
                }
            });
            addItem(new Action("新标签中打开") {
                @Override
                void onClick() {
                    LoadUrlEvent.post(mTab.getCurrentPageInfo(), true, false);
                }
            });

            addItem(new Action("无痕标签中打开") {
                @Override
                void onClick() {
                    LoadUrlEvent.post(mTab.getCurrentPageInfo(), true, true);
                }
            });
        }


        addItem(new Action("克隆标签") {
            @Override
            void onClick() {
                ITab cloneTab = TabGroupManager.cloneTab(mTab);
                if (cloneTab == null) {
                    ZToast.error("克隆标签失败！");
                } else {
                    ZToast.success("TODO 克隆标签成功！");
                }
            }
        });

        if (!isGroup) {
            addItem(new Action("悬浮模式") {
                @Override
                void onClick() {
                    ZToast.normal("TODO 悬浮模式");
//                GetActivityEvent.post(new Callback<ChromeActivity>() {
//                    @Override
//                    public void onResult(ChromeActivity result) {
//                        new FloatingTab(result, mTab).show();
//                    }
//                });
                }
            });

            addItem(new Action("历史栈") {
                @Override
                void onClick() {
                    HistoryStackDialogFragment.newInstance(mTab.getId()).show(context);
                }
            });
        }


        addItem(new Action("移动至群组") {
            @Override
            void onClick() {
                start(GroupTabPickerDialog.newInstance(mTab.getId()));
            }
        });


        if (!isGroup) {
            boolean canMoveTab = false;
            if (mTab instanceof IPageGroup) {
                canMoveTab = ((IPageGroup) mTab).getPages().size() > 1;
            }

            if (canMoveTab) {
                addItem(new Action("移动至新标签") {
                    @Override
                    void onClick() {
                        boolean r = TabGroupManager.moveToNewTab(mTab, mTab.getCurrentPage());
                        if (r) {
                            ZToast.success("移动页面成功！");
                        } else {
                            ZToast.error("移动页面失败！");
                        }
                    }
                });
            }

            addItem(new Action("网页设置") {
                @Override
                void onClick() {
                    start(SingleWebsiteFragment.newInstance(mTab.getCurrentPageInfo()));
                }
            });
        }
        super.initView(view, savedInstanceState);

        CardView cardView = findViewById(org.chromium.chrome.R.id.cv_container);
        cardView.setElevation(ScreenUtils.dp2pxInt(4));
    }

    @Override
    public void onSelect(AttachListDialogFragment<Action> fragment, int position, Action action) {
        action.onClick();
        fragment.dismiss();
    }

    public TabActionDialog setRenameCallback(Callback<String> callback) {
        mRenameCallback = callback;
        return this;
    }
}

