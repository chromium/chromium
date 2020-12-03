package org.chromium.chrome.browser.shopping_tiles;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.v2.FeedListContentManager;
import org.chromium.ui.modelutil.PropertyModel;

public class ShoppingProductContent extends FeedListContentManager.NativeViewContent {
    private PropertyModel mModel;
    private TextView mTextView;

    public ShoppingProductContent(String key, int resId, PropertyModel productInfoModel) {
        super(key, resId);

        mModel = productInfoModel;
    }

    /** Holds an inflated native view. */
    public ShoppingProductContent(String key, View nativeView) {
        super(key, nativeView);
    }

    @Override
    public View getNativeView(ViewGroup parent) {
        FrameLayout enclosingLayout = (FrameLayout) super.getNativeView(parent);

        if (getNestedView().getId() == R.id.for_you_view) {
            // Allows recycler view to function normally with a define height.
            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
            enclosingLayout.setLayoutParams(layoutParams);
        }

        return enclosingLayout;
    }

    public void bind(View view) {
        assert view instanceof TextView;
        ((TextView) view).setText("Shopping!");
    }
}
