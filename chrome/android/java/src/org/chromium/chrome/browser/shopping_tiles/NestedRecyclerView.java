package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is a nested RecyclerView that can be used within a Recyclerview. It controls the dispatching
 * of the scroll events. The scroll event can be consumed by this RecyclerView or dispatch to its
 * parent RecyclerView.
 *
 * This nested RecyclerView is the last item in the parent RecyclerView adapter. The parent
 * RecyclerView is consider settled if this last item is at position(0,0).
 *
 * Within this nested RecyclerView, the touch events are consumed if:
 *   * The parent is settled and user tries to scroll up Or
 *   * The parent is settled and user scrolls down when the first item in this nested
 *   *   RecyclerView is not completed visible.
 * Otherwise, the event is sent to the parent.
 */
public class NestedRecyclerView extends RecyclerView {
    @IntDef({ScrollingDirection.NONE, ScrollingDirection.UP, ScrollingDirection.DOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollingDirection {
        int NONE = 0;
        int UP = 1;
        int DOWN = 2;
    }

    private RecyclerView mParent;
    private int mTouchSlop;

    private float mDownY;
    private MotionEvent mDownEvent;

    @ScrollingDirection
    private int mScrollingDirection;

    private boolean mHasDispatched;

    public NestedRecyclerView(@NonNull Context context) {
        super(context);
    }

    public void setParentRecycler(RecyclerView parent) {
        mParent = parent;

        if (parent != null) {
            mTouchSlop = ViewConfiguration.get(parent.getContext()).getScaledTouchSlop();
        }
    }

    private boolean dispatchScroll(MotionEvent event) {
        assert mParent != null : "Parent recycler view must be set";

        final boolean firstItemCompletelyVisible = findViewHolderForAdapterPosition(0) != null
                && getLayoutManager().isViewPartiallyVisible(
                        findViewHolderForAdapterPosition(0).itemView, true, true);
        Log.d("Meil", "First item showing: " + firstItemCompletelyVisible);

        boolean parentIsSettle = mParent.findChildViewUnder(0, 0) == this.getParent();

        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                mHasDispatched = false;
                mScrollingDirection = ScrollingDirection.NONE;
                mParent.requestDisallowInterceptTouchEvent(true);

                mDownY = event.getY();
                mDownEvent = MotionEvent.obtain(event);
                return false;
            case MotionEvent.ACTION_MOVE:
                float deltaY = mDownY - event.getY();

                if (deltaY < -mTouchSlop) {
                    mScrollingDirection = ScrollingDirection.DOWN;
                } else if (deltaY > mTouchSlop) {
                    mScrollingDirection = ScrollingDirection.UP;
                }

                boolean scrollTowardFirstItem = mScrollingDirection == ScrollingDirection.DOWN;
                Log.e("Meil", "scrollTowardFirstItem: " + scrollTowardFirstItem);

                if (!parentIsSettle || (scrollTowardFirstItem && firstItemCompletelyVisible)) {
                    Log.e("Meil", "parent should scroll");
                    mParent.onTouchEvent(mDownEvent);
                    mParent.requestDisallowInterceptTouchEvent(false);
                    mHasDispatched = true;
                } else {
                    Log.e("Meil", "child should scroll, no parent movement");
                }
                return !mHasDispatched;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                return !mHasDispatched;
            default:
                return true;
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        dispatchScroll(event);
        boolean intercept = super.onInterceptTouchEvent(event);
        Log.e("Meil", "onIntercept touch event: " + intercept);
        return intercept;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        dispatchScroll(event);
        boolean onTouch = super.onTouchEvent(event);
        Log.e("Meil", "on touch event: " + onTouch);
        return onTouch;
    }
}
