package com.ark.browser.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.ui.widget.homepage.SwitcherRecyclerLayout;

import org.chromium.chrome.R;

public class BottomControlBar extends FrameLayout {

    private static final String TAG = "BottomControlBar";

    private final int mTouchSlop;
    private final int maxV;

    private VelocityTracker mTracker;


    private float mDownX;
    private float mDownY;

    private boolean isUp;
    private boolean isMoved;
    private boolean isDragToSwitch;

    private SwitcherRecyclerLayout mSwitcher;

    public BottomControlBar(@NonNull Context context) {
        this(context, null);
    }

    public BottomControlBar(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BottomControlBar(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        ViewConfiguration configuration = ViewConfiguration.get(context);
        maxV = configuration.getScaledMaximumFlingVelocity();
        mTouchSlop = configuration.getScaledTouchSlop();

        LayoutInflater.from(context).inflate(R.layout.layout_controller, this, true);

    }

    public void setSwitcher(SwitcherRecyclerLayout switcher) {
        mSwitcher = switcher;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {

        if (mSwitcher == null) {
            return super.onInterceptTouchEvent(event);
        }

        Log.d(TAG, "onInterceptTouchEvent event=" + MotionEvent.actionToString(event.getAction()));
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                isMoved = false;
                isUp = false;
                isDragToSwitch = false;
                if (mTracker == null) {
                    mTracker = VelocityTracker.obtain();
                }
                mTracker.addMovement(event);

                mDownX = event.getRawX();
                mDownY = event.getRawY();
                PageSnapshotManager.getInstance().cacheCurrentPage();
                break;
            case MotionEvent.ACTION_MOVE:
                if (isMoved) {
                    Log.d(TAG, "onInterceptTouchEvent event=" + MotionEvent.actionToString(event.getAction()) + " movemove");
                    return true;
                }

                float deltaX = event.getRawX() - mDownX;
                float deltaY = event.getRawY() - mDownY;
                if (Math.abs(deltaX) < mTouchSlop && Math.abs(deltaY) < mTouchSlop) { // 点击事件，交由内部处理
                    Log.d(TAG, "onInterceptTouchEvent event=" + MotionEvent.actionToString(event.getAction()) + " mTouchSlop");
                    return false;
                }

                Log.d(TAG, "onInterceptTouchEvent event=" + MotionEvent.actionToString(event.getAction()) + " movemove");
                isMoved = true;
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:

                if (isMoved) {
                    return true;
                }

                mTracker.clear();
                mTracker.recycle();
                mTracker = null;
                break;

        }

        return false;

    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mSwitcher == null) {
            return super.onTouchEvent(event);
        }
        Log.d(TAG, "onTouchEvent event=" + MotionEvent.actionToString(event.getAction()));
        if (mTracker == null) {
            mTracker = VelocityTracker.obtain();
        }
        mTracker.addMovement(event);
        switch (event.getAction()) {
            case MotionEvent.ACTION_MOVE:
                float deltaX = event.getRawX() - mDownX;
                float deltaY = event.getRawY() - mDownY;
                if (!isUp && (isDragToSwitch || deltaY > 0 || Math.abs(deltaY) < Math.abs(deltaX))) {
                    isUp = false;
                    isMoved = true;
                    isDragToSwitch = true;

                    mSwitcher.setVisibility(VISIBLE);
                    mSwitcher.dragToSwitchTab(deltaX, deltaY);
                    break;
                }
                // 取消TabSwitcherButton的长按
//                mTabSwitcherButton.cancelLongPress();
//                menuButton.cancelLongPress();
                isMoved = true;
                isUp = true;
                mSwitcher.setVisibility(VISIBLE);
                mSwitcher.moveDrag(deltaX, deltaY);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:

                isDragToSwitch = false;
                if (isMoved) {
                    mTracker.computeCurrentVelocity(1000, maxV);
                    mSwitcher.setVisibility(VISIBLE);
                    if (isUp) {
                        mSwitcher.endDrag(mTracker.getXVelocity(), mTracker.getYVelocity());
                    } else {
                        mSwitcher.endDragToSwitchTab(mTracker.getXVelocity());
                    }
                    mTracker.clear();
                    mTracker.recycle();
                    mTracker = null;
                }
                isMoved = false;
                isUp = false;
                isDragToSwitch = false;

                break;
        }
        return true;
    }


}
