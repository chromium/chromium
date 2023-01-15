package com.ark.browser.ui.fragment.base;


import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.core.ArkWindowAndroid;
import com.zpj.fragmentation.SimpleFragment;
import com.zpj.fragmentation.swipeback.SwipeBackLayout;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.chrome.R;

public abstract class BaseFragment extends SimpleFragment {

    protected ZToolBar toolbar;

    public ArkWindowAndroid getWindowAndroid() {
        if (_mActivity instanceof ArkBrowserActivity) {
            return ((ArkBrowserActivity) _mActivity).getWindowAndroid();
        }
        throw new IllegalStateException("the activity of fragment is not An ArkBrowserActivity! _mActivity=" + _mActivity);
    }

    @SuppressLint("ResourceType")
    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        if (context == null) {
            context = getContext();
        }
        if (getLayoutId() > 0) {
            view = inflater.inflate(getLayoutId(), container, false);
            toolbar = view.findViewById(R.id.tool_bar);
            CharSequence title = getToolbarTitle(context);
            if (title != null) {
                setToolbarTitle(title);
            }
            CharSequence subTitle = getToolbarSubTitle(context);
            if (title != null) {
                setToolbarSubTitle(subTitle);
            }
            initView(view, savedInstanceState);
            if (toolbar != null) {
                if (toolbar.getLeftImageButton() != null) {
                    toolbarLeftImageButton(toolbar.getLeftImageButton());
                } else if (toolbar.getLeftCustomView() != null) {
                    toolbarLeftCustomView(toolbar.getLeftCustomView());
                } else if (toolbar.getLeftTextView() != null) {
                    toolbarLeftTextView(toolbar.getLeftTextView());
                }
                if (toolbar.getRightImageButton() != null) {
                    toolbarRightImageButton(toolbar.getRightImageButton());
                } else if (toolbar.getRightCustomView() != null) {
                    toolbarRightCustomView(toolbar.getRightCustomView());
                } else if (toolbar.getRightTextView() != null) {
                    toolbarRightTextView(toolbar.getRightTextView());
                }

                if (toolbar.getCenterTextView() != null) {
                    toolbarCenterTextView(toolbar.getCenterTextView());
                } else if (toolbar.getCenterSubTextView() != null) {
                    toolbarCenterSubTextView(toolbar.getCenterSubTextView());
                } else if (toolbar.getCenterCustomView() != null) {
                    toolbarCenterCustomView(toolbar.getCenterCustomView());
                }
            }
            if (view != null && supportSwipeBack()) {
                SwipeBackLayout.EdgeLevel level = getEdgeLevel();
                setEdgeLevel(level == null ? SwipeBackLayout.EdgeLevel.MAX : level);
                FrameLayout flContainer = new FrameLayout(context);
                flContainer.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                flContainer.addView(attachToSwipeBack(view));
                return flContainer;
            } else {
                return view;
            }
        } else {
            view = super.onCreateView(inflater, container, savedInstanceState);
            return view;
        }
    }

    protected abstract void initView(View view, @Nullable Bundle savedInstanceState);

    public CharSequence getToolbarTitle(Context context) {
        if (getToolbarTitleId() > 0) {
            return context.getResources().getString(getToolbarTitleId());
        }
        return null;
    }

    public int getToolbarTitleId() {
        return 0;
    }

    public CharSequence getToolbarSubTitle(Context context) {
        if (getToolbarSubTitleId() > 0) {
            return context.getResources().getString(getToolbarSubTitleId());
        }
        return null;
    }

    public int getToolbarSubTitleId() {
        return 0;
    }

    public void toolbarLeftImageButton(@NonNull ImageButton imageButton) {
        imageButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pop();
            }
        });
    }

    public void toolbarLeftCustomView(@NonNull View view) {

    }

    public void toolbarLeftTextView(@NonNull TextView view) {

    }

    public void toolbarRightImageButton(@NonNull ImageButton imageButton) {

    }

    public void toolbarRightCustomView(@NonNull View view) {

    }

    public void toolbarRightTextView(@NonNull TextView view) {

    }

    public void toolbarCenterTextView(@NonNull TextView view) {

    }

    public void toolbarCenterSubTextView(@NonNull TextView view) {

    }

    public void toolbarCenterCustomView(@NonNull View view) {

    }

    public void setToolbarTitle(@StringRes int titleRes) {
        if (toolbar != null && toolbar.getCenterTextView() != null) {
            toolbar.getCenterTextView().setText(titleRes);
        }
    }

    public void setToolbarSubTitle(@StringRes int titleRes) {
        if (toolbar != null && toolbar.getCenterSubTextView() != null) {
            toolbar.getCenterSubTextView().setText(titleRes);
        }
    }

    public void setToolbarTitle(CharSequence title) {
        if (toolbar != null && toolbar.getCenterTextView() != null) {
            toolbar.getCenterTextView().setText(title);
        }
    }

    public void setToolbarSubTitle(CharSequence title) {
        if (toolbar != null && toolbar.getCenterSubTextView() != null) {
            toolbar.getCenterSubTextView().setText(title);
        }
    }

}
