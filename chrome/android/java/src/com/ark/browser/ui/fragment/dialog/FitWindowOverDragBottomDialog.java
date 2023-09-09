package com.ark.browser.ui.fragment.dialog;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.ark.browser.utils.WindowDisplayFrameObserver;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;

import org.chromium.base.Log;

public abstract class FitWindowOverDragBottomDialog<T extends FitWindowOverDragBottomDialog<T>>
        extends OverDragBottomDialogFragment<T> {

    protected WindowDisplayFrameObserver mWindowDisplayFrameObserver;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mWindowDisplayFrameObserver = new WindowDisplayFrameObserver(context, new org.chromium.base.Callback<Rect>() {

            private int mTempBottom = -1;

            @Override
            public void onResult(Rect rect) {
                if (contentView != null) {
                    if (mTempBottom == rect.bottom) {
                        return;
                    }
                    mTempBottom = rect.bottom;
                    int decorHeight = _mActivity.getWindow().getDecorView().getHeight();
                    int diff = rootView.getHeight() - rect.bottom;
                    Log.e("SiteRedirectEditorDialog", "diff=" + diff
                            + " height=" + rootView.getHeight()
                            + " decorHeight=" + decorHeight
                            + " bottom=" + rect.bottom);
                    ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) contentView.getLayoutParams();
                    int originalMarginBottom = getMarginBottom();
                    if (diff > 0) {
//                        if (mWrapper.getPaddingBottom() != diff) {
//                            mWrapper.setPadding(0, 0, 0, diff);
//                        }
                        if (params.bottomMargin - originalMarginBottom != diff) {
                            params.bottomMargin = diff + originalMarginBottom;
                        }
                    } else {
                        params.bottomMargin = originalMarginBottom;
//                        mWrapper.setPadding(0, 0, 0, 0);
                    }
                    contentView.setLayoutParams(params);
                }
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mWindowDisplayFrameObserver != null) {
            mWindowDisplayFrameObserver.enable();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mWindowDisplayFrameObserver != null) {
            mWindowDisplayFrameObserver.disable();
        }
    }

    @Override
    public void onDestroyView() {
        if (mWindowDisplayFrameObserver != null) {
            mWindowDisplayFrameObserver.disable();
        }
        super.onDestroyView();
    }

}
